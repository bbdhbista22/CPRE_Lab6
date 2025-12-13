force sim:/piped_mac/ACLK 1 0, 0 {5 ps} -r 10
run 20
force sim:/piped_mac/ARESETN 1 0 -cancel 20
force sim:/piped_mac/ARESETN 0 20
run 30

# No back pressure
force sim:/piped_mac/MO_AXIS_TREADY 1 0


# 1. Basic Single Operation
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TID 16#1 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0101 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 50
# Expected:
#   0x0000_0001

# 2. Basic Double Operation
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 0 0
force sim:/piped_mac/SD_AXIS_TID 16#2 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0101 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0101 0
run 10
force sim:/piped_mac/SD_AXIS_TLAST 0 0
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 50
# Expected:
#   0x0000_0002


# 3. Basic Single Operation back-to-back
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TID 16#3 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0203 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TID 16#4 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0202 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 50
# Expected:
#   0x0000_0006
#   0x0000_0004


# 4. Inital Load Single Operation ********
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 0 0
force sim:/piped_mac/SD_AXIS_TID 16#5 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0001 0
force sim:/piped_mac/SD_AXIS_TUSER 1 0
run 10
force sim:/piped_mac/SD_AXIS_TID 16#6 0
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0303 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 50
# Expected:
#   0x0000_000A *****


# 5. Basic Multi Operation
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 0 0
force sim:/piped_mac/SD_AXIS_TID 16#7 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0101 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TID 16#8 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0102 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 30
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TID 16#9 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0203 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 50 
# Expected:
#   0x0000_0009


# Apply Back Pressure
force sim:/piped_mac/MO_AXIS_TREADY 1 0
run 10

# 6. Basic Single Operation
force sim:/piped_mac/SD_AXIS_TVALID 1 0
force sim:/piped_mac/SD_AXIS_TLAST 1 0
force sim:/piped_mac/SD_AXIS_TID 16#A 0
force sim:/piped_mac/SD_AXIS_TDATA 16#0204 0
force sim:/piped_mac/SD_AXIS_TUSER 0 0
run 10
force sim:/piped_mac/SD_AXIS_TVALID 0 0
run 30
# Aleaviate Back Pressure
force sim:/piped_mac/MO_AXIS_TREADY 0 0
run 50
# Expected:
#   0x0000_0008
