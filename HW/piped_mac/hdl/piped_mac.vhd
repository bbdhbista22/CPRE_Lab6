-------------------------------------------------------------------------
-- Matthew Dwyer
-- Department of Electrical and Computer Engineering
-- Iowa State University
-------------------------------------------------------------------------
-- piped_mac.vhd
-------------------------------------------------------------------------
-- DESCRIPTION: This file contains a basic piplined axi-stream mac unit. It
-- multiplies two integer values togeather and accumulates them.
--
-- NOTES:
-- 10/25/21 by MPD::Inital template creation
-- 9/5/25 by CWS::Minor changes to remove Qx.x
-------------------------------------------------------------------------

LIBRARY work;
LIBRARY IEEE;
USE IEEE.std_logic_1164.ALL;
USE IEEE.numeric_std.ALL;

ENTITY piped_mac IS
    GENERIC (
        -- Parameters of mac
        C_DATA_WIDTH : INTEGER := 8
    );
    PORT (
        ACLK : IN STD_LOGIC;
        ARESETN : IN STD_LOGIC;

        -- AXIS slave data interface
        SD_AXIS_TREADY : OUT STD_LOGIC;
        SD_AXIS_TDATA : IN STD_LOGIC_VECTOR(C_DATA_WIDTH * 2 - 1 DOWNTO 0); -- Packed data input
        SD_AXIS_TLAST : IN STD_LOGIC;
        SD_AXIS_TUSER : IN STD_LOGIC; -- Should we treat this first value in the stream as an inital accumulate value?
        SD_AXIS_TVALID : IN STD_LOGIC;
        SD_AXIS_TID : IN STD_LOGIC_VECTOR(7 DOWNTO 0);

        -- AXIS master accumulate result out interface
        MO_AXIS_TVALID : OUT STD_LOGIC;
        MO_AXIS_TDATA : OUT STD_LOGIC_VECTOR(31 DOWNTO 0);
        MO_AXIS_TLAST : OUT STD_LOGIC;
        MO_AXIS_TREADY : IN STD_LOGIC;
        MO_AXIS_TID : OUT STD_LOGIC_VECTOR(7 DOWNTO 0)
    );

    ATTRIBUTE SIGIS : STRING;
    ATTRIBUTE SIGIS OF ACLK : SIGNAL IS "Clk";

END piped_mac;
ARCHITECTURE behavioral OF piped_mac IS
    -- Internal Signals
    -- Stage 0 registers (input capture)
    SIGNAL s0_valid : STD_LOGIC := '0';
    SIGNAL s0_tdata : STD_LOGIC_VECTOR(C_DATA_WIDTH * 2 - 1 DOWNTO 0) := (OTHERS => '0');
    SIGNAL s0_tlast : STD_LOGIC := '0';
    SIGNAL s0_tuser : STD_LOGIC := '0';
    SIGNAL s0_tid : STD_LOGIC_VECTOR(7 DOWNTO 0) := (OTHERS => '0');

    -- Stage 1 registers (multiply results)
    SIGNAL s1_valid : STD_LOGIC := '0';
    SIGNAL s1_product : signed((C_DATA_WIDTH * 2) - 1 DOWNTO 0) := (OTHERS => '0');
    SIGNAL s1_tdata : STD_LOGIC_VECTOR(C_DATA_WIDTH * 2 - 1 DOWNTO 0) := (OTHERS => '0');
    SIGNAL s1_tlast : STD_LOGIC := '0';
    SIGNAL s1_tuser : STD_LOGIC := '0';
    SIGNAL s1_tid : STD_LOGIC_VECTOR(7 DOWNTO 0) := (OTHERS => '0');

    -- Accumulator / output registers
    SIGNAL accumulator : signed(31 DOWNTO 0) := (OTHERS => '0');
    SIGNAL result_reg : signed(31 DOWNTO 0) := (OTHERS => '0');
    SIGNAL result_tid : STD_LOGIC_VECTOR(7 DOWNTO 0) := (OTHERS => '0');
    SIGNAL result_valid : STD_LOGIC := '0';

    -- Ready/valid helper signals
    SIGNAL s0_ready : STD_LOGIC;
    SIGNAL s1_ready : STD_LOGIC;
    SIGNAL s2_block : STD_LOGIC;
    SIGNAL s2_accept : STD_LOGIC;
    SIGNAL clear_result : STD_LOGIC;

    -- Debug signals, make sure we aren't going crazy
    SIGNAL mac_debug : STD_LOGIC_VECTOR(31 DOWNTO 0);

BEGIN

    -- Interface signals
    SD_AXIS_TREADY <= s0_ready;

    MO_AXIS_TVALID <= result_valid;
    MO_AXIS_TDATA <= STD_LOGIC_VECTOR(result_reg);
    MO_AXIS_TLAST <= result_valid;
    MO_AXIS_TID <= result_tid;

    -- Ready logic
    s2_block <= '1' WHEN (s1_valid = '1' AND s1_tlast = '1' AND result_valid = '1' AND MO_AXIS_TREADY = '0') ELSE
        '0';
    s2_accept <= '1' WHEN (s1_valid = '1' AND s2_block = '0') ELSE
        '0';

    s1_ready <= '1' WHEN (s1_valid = '0') OR (s2_accept = '1') ELSE
        '0';
    s0_ready <= '1' WHEN (s0_valid = '0') OR (s1_ready = '1') ELSE
        '0';

    clear_result <= '1' WHEN (result_valid = '1' AND MO_AXIS_TREADY = '1') ELSE
        '0';

    -- Internal signals

    -- Debug Signals
    mac_debug <= STD_LOGIC_VECTOR(accumulator); -- Track accumulator for sanity

    PROCESS (ACLK) IS
        VARIABLE load_value : signed(31 DOWNTO 0);
        VARIABLE product_ext : signed(31 DOWNTO 0);
        VARIABLE next_accum : signed(31 DOWNTO 0);
        VARIABLE result_valid_next : STD_LOGIC;
    BEGIN
        IF rising_edge(ACLK) THEN -- Rising Edge

            -- Reset values if reset is low
            IF ARESETN = '0' THEN -- Reset
                s0_valid <= '0';
                s1_valid <= '0';
                result_valid <= '0';
                accumulator <= (OTHERS => '0');
                result_reg <= (OTHERS => '0');
                result_tid <= (OTHERS => '0');
                s0_tdata <= (OTHERS => '0');
                s0_tlast <= '0';
                s0_tuser <= '0';
                s0_tid <= (OTHERS => '0');
                s1_product <= (OTHERS => '0');
                s1_tdata <= (OTHERS => '0');
                s1_tlast <= '0';
                s1_tuser <= '0';
                s1_tid <= (OTHERS => '0');
            ELSE
                result_valid_next := result_valid;
                IF clear_result = '1' THEN
                    result_valid_next := '0';
                END IF;

                -- Stage 0: capture incoming AXIS beat
                IF s0_ready = '1' THEN
                    s0_valid <= SD_AXIS_TVALID;
                    IF SD_AXIS_TVALID = '1' THEN
                        s0_tdata <= SD_AXIS_TDATA;
                        s0_tlast <= SD_AXIS_TLAST;
                        s0_tuser <= SD_AXIS_TUSER;
                        s0_tid <= SD_AXIS_TID;
                    END IF;
                END IF;

                -- Stage 1: multiply operands
                IF s1_ready = '1' THEN
                    s1_valid <= s0_valid;
                    IF s0_valid = '1' THEN
                        s1_tdata <= s0_tdata;
                        s1_tlast <= s0_tlast;
                        s1_tuser <= s0_tuser;
                        s1_tid <= s0_tid;
                        s1_product <= signed(s0_tdata(C_DATA_WIDTH * 2 - 1 DOWNTO C_DATA_WIDTH)) *
                            signed(s0_tdata(C_DATA_WIDTH - 1 DOWNTO 0));
                    END IF;
                END IF;

                -- Stage 2: accumulation and result capture
                IF s2_accept = '1' THEN
                    load_value := resize(signed(s1_tdata), 32);
                    product_ext := resize(s1_product, 32);
                    IF s1_tuser = '1' THEN
                        next_accum := load_value;
                    ELSE
                        next_accum := accumulator + product_ext;
                    END IF;

                    IF s1_tlast = '1' THEN
                        accumulator <= (OTHERS => '0');
                        result_reg <= next_accum;
                        result_tid <= s1_tid;
                        result_valid_next := '1';
                    ELSE
                        accumulator <= next_accum;
                    END IF;
                END IF;

                result_valid <= result_valid_next;
            END IF; -- Reset/Else

        END IF; -- Rising Edge
    END PROCESS;
END ARCHITECTURE behavioral;