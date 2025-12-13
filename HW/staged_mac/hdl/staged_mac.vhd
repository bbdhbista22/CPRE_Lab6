-------------------------------------------------------------------------
-- Matthew Dwyer
-- Department of Electrical and Computer Engineering
-- Iowa State University
-------------------------------------------------------------------------
-- staged_mac.vhd
-------------------------------------------------------------------------
-- DESCRIPTION: This file contains a basic staged axi-stream mac unit. It
-- multiplies two integer values together and accumulates them.
--
-- NOTES:
-- 10/25/21 by MPD::Inital template creation
-- 9/5/25 by CWS::Minor changes to remove Qx.x
-------------------------------------------------------------------------

LIBRARY work;
LIBRARY IEEE;
USE IEEE.std_logic_1164.ALL;
USE IEEE.numeric_std.ALL;

ENTITY staged_mac IS
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

END staged_mac;
ARCHITECTURE behavioral OF staged_mac IS
    -- Internal Signals
    TYPE STATE_TYPE IS (IDLE, ACCUMULATE, OUTPUT);
    SIGNAL state : STATE_TYPE := IDLE;
    SIGNAL accumulator : signed(31 DOWNTO 0) := (OTHERS => '0');
    SIGNAL result_reg : signed(31 DOWNTO 0) := (OTHERS => '0');
    SIGNAL tid_reg : STD_LOGIC_VECTOR(7 DOWNTO 0) := (OTHERS => '0');

    SIGNAL weight_in : signed(C_DATA_WIDTH - 1 DOWNTO 0);
    SIGNAL activation_in : signed(C_DATA_WIDTH - 1 DOWNTO 0);
    SIGNAL mac_product : signed((C_DATA_WIDTH * 2) - 1 DOWNTO 0);

    SIGNAL sd_axis_tready_i : STD_LOGIC;

    -- Debug signals, make sure we aren't going crazy
    SIGNAL mac_debug : STD_LOGIC_VECTOR(31 DOWNTO 0);

BEGIN

    -- Interface signals
    SD_AXIS_TREADY <= sd_axis_tready_i;

    MO_AXIS_TVALID <= '1' WHEN state = OUTPUT ELSE
        '0';
    MO_AXIS_TDATA <= STD_LOGIC_VECTOR(result_reg);
    MO_AXIS_TLAST <= '1' WHEN state = OUTPUT ELSE
        '0';
    MO_AXIS_TID <= tid_reg;

    -- Internal signals
    sd_axis_tready_i <= '1' WHEN state /= OUTPUT ELSE
        '0';

    weight_in <= signed(SD_AXIS_TDATA(C_DATA_WIDTH * 2 - 1 DOWNTO C_DATA_WIDTH));
    activation_in <= signed(SD_AXIS_TDATA(C_DATA_WIDTH - 1 DOWNTO 0));
    mac_product <= weight_in * activation_in;

    -- Debug Signals
    mac_debug <= STD_LOGIC_VECTOR(accumulator); -- Simple sanity visibility

    PROCESS (ACLK) IS
        VARIABLE next_accum : signed(31 DOWNTO 0);
        VARIABLE product_ext : signed(31 DOWNTO 0);
        VARIABLE load_value : signed(31 DOWNTO 0);
        VARIABLE input_fire : BOOLEAN;
    BEGIN
        IF rising_edge(ACLK) THEN -- Rising Edge

            -- Reset values if reset is low
            IF ARESETN = '0' THEN -- Reset
                state <= IDLE;
                accumulator <= (OTHERS => '0');
                result_reg <= (OTHERS => '0');
                tid_reg <= (OTHERS => '0');

            ELSE
                next_accum := accumulator;
                product_ext := resize(mac_product, 32);
                load_value := resize(signed(SD_AXIS_TDATA), 32);
                input_fire := (SD_AXIS_TVALID = '1') AND (sd_axis_tready_i = '1');

                CASE state IS -- State
                    WHEN IDLE | ACCUMULATE =>
                        IF input_fire THEN
                            IF SD_AXIS_TUSER = '1' THEN
                                next_accum := load_value;
                            ELSE
                                next_accum := accumulator + product_ext;
                            END IF;

                            accumulator <= next_accum;

                            IF SD_AXIS_TLAST = '1' THEN
                                result_reg <= next_accum;
                                tid_reg <= SD_AXIS_TID;
                                state <= OUTPUT;
                            ELSE
                                state <= ACCUMULATE;
                            END IF;
                        END IF;
                        -- Other stages go here	

                    WHEN OUTPUT =>
                        IF MO_AXIS_TREADY = '1' THEN
                            state <= IDLE;
                            accumulator <= (OTHERS => '0');
                        END IF;
                END CASE; -- State
            END IF; -- Reset

        END IF; -- Rising Edge
    END PROCESS;
END ARCHITECTURE behavioral;