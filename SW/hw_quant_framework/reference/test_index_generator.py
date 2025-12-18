#!/usr/bin/env python3
"""
IndexGenerator Test - Pure Python Implementation for Validation
This serves as a reference implementation to verify the C++ IndexGenerator logic
"""

import sys
import math

class IndexGenerator:
    """Python version of IndexGenerator for testing and validation"""
    
    def __init__(self, config, input_base_addr=0, weight_base_addr=0, tile_size=16):
        self.conv_config = config.copy()
        self.input_base_addr = input_base_addr
        self.weight_base_addr = weight_base_addr
        self.tile_size = tile_size
        
        # Validate configuration
        if (config['input_height'] == 0 or config['input_width'] == 0 or 
            config['input_channels'] == 0):
            raise ValueError("Invalid input dimensions")
        if (config['filter_height'] == 0 or config['filter_width'] == 0 or 
            config['num_filters'] == 0):
            raise ValueError("Invalid filter dimensions")
        
        # Compute derived values
        self.conv_config['output_height'] = (
            (config['input_height'] - config['filter_height'] + 2 * config['padding']) 
            // config['stride'] + 1
        )
        self.conv_config['output_width'] = (
            (config['input_width'] - config['filter_width'] + 2 * config['padding']) 
            // config['stride'] + 1
        )
        self.conv_config['macs_per_pixel'] = (
            config['filter_height'] * config['filter_width'] * config['input_channels']
        )
        
        # Compute tiling configuration
        self.tiles_per_row = math.ceil(self.conv_config['output_width'] / tile_size)
        self.tiles_per_col = math.ceil(self.conv_config['output_height'] / tile_size)
        self.total_tiles = self.tiles_per_row * self.tiles_per_col
    
    def calc_input_addr(self, in_y, in_x, ic):
        """Calculate input BRAM address"""
        offset = (in_y * self.conv_config['input_width'] + in_x) * self.conv_config['input_channels'] + ic
        return self.input_base_addr + offset
    
    def calc_weight_addr(self, oc, fy, fx, ic):
        """Calculate weight BRAM address"""
        offset = (oc * self.conv_config['filter_height'] * self.conv_config['filter_width'] 
                  * self.conv_config['input_channels']
                  + fy * self.conv_config['filter_width'] * self.conv_config['input_channels']
                  + fx * self.conv_config['input_channels']
                  + ic)
        return self.weight_base_addr + offset
    
    def calc_input_position(self, out_y, out_x, fy, fx):
        """Calculate input position from output position and filter offset"""
        temp_in_y = out_y * self.conv_config['stride'] - self.conv_config['padding'] + fy
        temp_in_x = out_x * self.conv_config['stride'] - self.conv_config['padding'] + fx
        
        # Check bounds
        if (temp_in_y < 0 or temp_in_y >= self.conv_config['input_height'] or
            temp_in_x < 0 or temp_in_x >= self.conv_config['input_width']):
            return None, None, False
        
        return temp_in_y, temp_in_x, True
    
    def generate_all_addresses(self):
        """Generate all address pairs for complete layer"""
        addresses = []
        
        num_output_channels = self.conv_config['num_filters']
        output_height = self.conv_config['output_height']
        output_width = self.conv_config['output_width']
        filter_height = self.conv_config['filter_height']
        filter_width = self.conv_config['filter_width']
        input_channels = self.conv_config['input_channels']
        
        # Process output channels in groups of 4
        for oc_batch in range((num_output_channels + 3) // 4):
            
            # Process each tile
            for tile_id in range(self.total_tiles):
                tile_row = tile_id // self.tiles_per_row
                tile_col = tile_id % self.tiles_per_row
                
                # Process each pixel within tile
                for out_y_in_tile in range(self.tile_size):
                    for out_x_in_tile in range(self.tile_size):
                        
                        actual_out_y = tile_row * self.tile_size + out_y_in_tile
                        actual_out_x = tile_col * self.tile_size + out_x_in_tile
                        
                        # Skip if outside output bounds
                        if actual_out_y >= output_height or actual_out_x >= output_width:
                            continue
                        
                        # Process 4 output channels in parallel
                        for oc_offset in range(4):
                            oc = oc_batch * 4 + oc_offset
                            
                            # Skip if this output channel doesn't exist
                            if oc >= num_output_channels:
                                continue
                            
                            # Generate MAC operations for this output pixel and channel
                            for fy in range(filter_height):
                                for fx in range(filter_width):
                                    for ic in range(input_channels):
                                        
                                        # Calculate input position
                                        in_y, in_x, valid = self.calc_input_position(
                                            actual_out_y, actual_out_x, fy, fx
                                        )
                                        
                                        # Always generate address (even for padding)
                                        input_addr = self.calc_input_addr(in_y or 0, in_x or 0, ic)
                                        weight_addr = self.calc_weight_addr(oc, fy, fx, ic)
                                        
                                        # TLAST asserted only on LAST MAC of this output pixel
                                        # = last input channel (ic == input_channels-1) AND
                                        #   last filter position (fy == filter_height-1 and fx == filter_width-1)
                                        tlast = (ic == input_channels - 1 and 
                                                fy == filter_height - 1 and 
                                                fx == filter_width - 1)
                                        
                                        addresses.append({
                                            'input_addr': input_addr,
                                            'weight_addr': weight_addr,
                                            'tlast': tlast,
                                            'oc': oc_offset
                                        })
        
        return addresses
    
    def generate_first_n(self, n):
        """Generate first N addresses"""
        all_addresses = self.generate_all_addresses()
        return all_addresses[:n]
    
    def verify_addresses(self, addresses):
        """Verify address sequence correctness"""
        if not addresses:
            print("ERROR: Empty address vector")
            return False
        
        expected_total_macs = (self.conv_config['output_height'] * 
                              self.conv_config['output_width'] * 
                              self.conv_config['num_filters'] * 
                              self.conv_config['macs_per_pixel'])
        
        # Check total count
        if len(addresses) != expected_total_macs:
            print(f"ERROR: Total MACs mismatch. Expected: {expected_total_macs}, Got: {len(addresses)}")
            return False
        
        # Check TLAST placement
        mac_count = 0
        for i, addr in enumerate(addresses):
            mac_count += 1
            expected_tlast = (mac_count % self.conv_config['macs_per_pixel']) == 0
            
            if addr['tlast'] != expected_tlast:
                print(f"ERROR: TLAST mismatch at address {i}. Expected: {expected_tlast}, Got: {addr['tlast']}")
                return False
        
        # Check address bounds
        max_input_addr = (self.input_base_addr + 
                         self.conv_config['input_height'] * self.conv_config['input_width'] * 
                         self.conv_config['input_channels'])
        max_weight_addr = (self.weight_base_addr + 
                          self.conv_config['num_filters'] * self.conv_config['filter_height'] * 
                          self.conv_config['filter_width'] * self.conv_config['input_channels'])
        
        for i, addr in enumerate(addresses):
            if addr['input_addr'] >= max_input_addr:
                print(f"ERROR: Input address out of bounds at index {i}. "
                      f"Address: 0x{addr['input_addr']:x}, Max: 0x{max_input_addr:x}")
                return False
            if addr['weight_addr'] >= max_weight_addr:
                print(f"ERROR: Weight address out of bounds at index {i}. "
                      f"Address: 0x{addr['weight_addr']:x}, Max: 0x{max_weight_addr:x}")
                return False
        
        # Check output channel index
        for i, addr in enumerate(addresses):
            if addr['oc'] > 3:
                print(f"ERROR: Invalid output channel at index {i}. OC: {addr['oc']}")
                return False
        
        print("✓ Address verification PASSED")
        print(f"  Total MACs: {len(addresses)}")
        print(f"  Expected: {expected_total_macs}")
        print(f"  TLAST placement: CORRECT (every {self.conv_config['macs_per_pixel']} MACs)")
        print(f"  Address bounds: OK")
        
        return True


def main():
    print("=" * 50)
    print("IndexGenerator Test - Conv1 Layer")
    print("=" * 50)
    print()
    
    # Conv1 configuration from theoretical_analysis.md
    config = {
        'input_height': 64,
        'input_width': 64,
        'input_channels': 3,
        'filter_height': 3,
        'filter_width': 3,
        'num_filters': 64,
        'stride': 1,
        'padding': 1
    }
    
    try:
        gen = IndexGenerator(config, input_base_addr=0, weight_base_addr=0, tile_size=16)
        
        print("Configuration:")
        print(f"  Input:       {config['input_height']}x{config['input_width']}x{config['input_channels']}")
        print(f"  Filter:      {config['filter_height']}x{config['filter_width']}x{config['input_channels']} "
              f"(stride={config['stride']}, padding={config['padding']})")
        print(f"  Output:      {gen.conv_config['output_height']}x{gen.conv_config['output_width']}x{config['num_filters']}")
        print(f"  MACs/pixel:  {gen.conv_config['macs_per_pixel']}")
        print(f"  Tile size:   {gen.tile_size}x{gen.tile_size}")
        print(f"  Tiles:       {gen.tiles_per_row}x{gen.tiles_per_col} ({gen.total_tiles} total)")
        print()
        
        # Calculate expected total MACs
        expected_macs = (gen.conv_config['output_height'] * 
                        gen.conv_config['output_width'] * 
                        config['num_filters'] * 
                        gen.conv_config['macs_per_pixel'])
        
        print(f"Expected total MACs: {expected_macs}")
        print(f"  = {gen.conv_config['output_height']} × {gen.conv_config['output_width']} × "
              f"{config['num_filters']} × {gen.conv_config['macs_per_pixel']}")
        print(f"  = {expected_macs} (should be 7,077,888)")
        print()
        
        # Generate first 100 addresses
        print("Generating first 100 addresses...")
        print()
        
        first_100 = gen.generate_first_n(100)
        
        print(f"{'Idx':>5} | {'Input':>8} | {'Weight':>8} | TLAST | OC")
        print("-" * 50)
        
        for i, addr in enumerate(first_100):
            print(f"{i:>5} | 0x{addr['input_addr']:06x} | 0x{addr['weight_addr']:06x} | "
                  f"{'Y' if addr['tlast'] else 'N':>5} | {addr['oc']}")
            
            # Print separator every 27 MACs
            if (i + 1) % 27 == 0:
                print("-" * 50)
        
        # Test TLAST pattern
        print("\nTLAST Pattern Verification:")
        tlast_count = sum(1 for addr in first_100 if addr['tlast'])
        print(f"  First 100 MACs: {tlast_count} TLAST signals")
        print(f"  Expected: {100 // 27} TLAST signals")
        print(f"  Pattern: TLAST should appear every {gen.conv_config['macs_per_pixel']} MACs")
        print()
        
        # Generate all addresses and verify
        print("Generating all addresses and verifying...")
        all_addresses = gen.generate_all_addresses()
        
        if gen.verify_addresses(all_addresses):
            print("\n✓ All tests PASSED!")
            print("\n" + "=" * 50)
            print("Summary:")
            print("=" * 50)
            print(f"✓ IndexGenerator correctly generates {len(all_addresses)} address pairs")
            print(f"✓ TLAST signals placed correctly (every {gen.conv_config['macs_per_pixel']} MACs)")
            print(f"✓ All addresses within valid bounds")
            print(f"✓ Ready for VHDL testbench validation")
            return 0
        else:
            print("\n✗ Verification FAILED!")
            return 1
    
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
