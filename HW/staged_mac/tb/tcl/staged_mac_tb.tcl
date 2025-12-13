relaunch_sim

# Clock and reset
add_force {/staged_mac/ACLK} -radix hex {0 0ns} {1 5000ps} -repeat_every 10000ps
add_force {/staged_mac/ARESETN} -radix hex {0 0ns}
run 30ns
add_force {/staged_mac/ARESETN} -radix hex {1 0ns}
# No back pressure
add_force {/staged_mac/MO_AXIS_TREADY} -radix hex {1 0ns}


# 1. Basic Single Operation
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0101 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 50ns
# Expected:
#   0x0000_0001

# 2. Basic Double Operation
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {0 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {2 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0101 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0101 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {0 0ns}
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 50ns
# Expected:
#   0x0000_0002


# 3. Basic Single Operation back-to-back
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {3 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0203 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {4 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0202 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 50ns
# Expected:
#   0x0000_0006
#   0x0000_0004


# 4. Inital Load Single Operation ********
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {0 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {5 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0001 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {1 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TID} -radix hex {6 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0303 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 50ns
# Expected:
#   0x0000_000A *****


# 5. Basic Multi Operation
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {0 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {7 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0101 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {8 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0102 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 30ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {9 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0203 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 50 ns
# Expected:
#   0x0000_0009


# Apply Back Pressure
add_force {/staged_mac/MO_AXIS_TREADY} -radix hex {1 0ns}
run 10ns

# 6. Basic Single Operation
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TLAST} -radix hex {1 0ns}
add_force {/staged_mac/SD_AXIS_TID} -radix hex {A 0ns}
add_force {/staged_mac/SD_AXIS_TDATA} -radix hex {0204 0ns}
add_force {/staged_mac/SD_AXIS_TUSER} -radix hex {0 0ns}
run 10ns
add_force {/staged_mac/SD_AXIS_TVALID} -radix hex {0 0ns}
run 30ns
# Aleaviate Back Pressure
add_force {/staged_mac/MO_AXIS_TREADY} -radix hex {0 0ns}
run 50ns
# Expected:
#   0x0000_0008

