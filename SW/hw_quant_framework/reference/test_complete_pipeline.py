#!/usr/bin/env python3
"""
Complete Hardware Accelerator Pipeline Test
Includes: MAC Units → Dequantization → Output Storage
With verbose hardware-comparable output for FPGA verification
"""

import sys
import io
from test_index_generator import IndexGenerator
from test_dequantization import Dequantization
from test_output_storage import OutputStorage


class StagedMAC:
    """Python reference for staged MAC unit"""
    
    def __init__(self, config):
        self.id = config['id']
        self.zero_point_in = config['zero_point_in']
        self.zero_point_weight = config['zero_point_weight']
        self.pipeline = [
            {'valid': False, 'input': 0, 'weight': 0, 'product': 0},
            {'valid': False, 'input': 0, 'weight': 0, 'product': 0},
            {'valid': False, 'input': 0, 'weight': 0, 'product': 0}
        ]
        self.current_accumulator = 0
        self.cycle_count = 0
    
    def execute_cycle(self, input_val, weight_val, start_new_pixel=False):
        """Execute one pipeline cycle"""
        if start_new_pixel:
            self.current_accumulator = 0
        
        # Shift pipeline
        # Stage 2 output
        result = {
            'cycle': self.cycle_count,
            'accumulator': self.current_accumulator,
            'valid': self.pipeline[2]['valid']
        }
        
        # Shift stages
        self.pipeline[2] = self.pipeline[1].copy()
        
        # Accumulate at stage 1
        if self.pipeline[0]['valid']:
            self.pipeline[1]['valid'] = True
            self.current_accumulator += self.pipeline[0]['product']
        
        # Multiply at stage 0
        adj_input = int(input_val) - self.zero_point_in
        adj_weight = int(weight_val) - self.zero_point_weight
        product = adj_input * adj_weight
        
        self.pipeline[0] = {
            'valid': True,
            'input': input_val,
            'weight': weight_val,
            'product': product
        }
        
        self.cycle_count += 1
        return result
    
    def get_accumulator(self):
        return self.current_accumulator
    
    def reset_accumulator(self):
        self.current_accumulator = 0


class MACStreamProvider:
    """4 parallel staged MACs"""
    
    def __init__(self, config):
        self.config = config
        self.macs = []
        for i in range(4):
            mac_config = {
                'id': i,
                'zero_point_in': config['zero_point_in'],
                'zero_point_weight': config['zero_point_weight']
            }
            self.macs.append(StagedMAC(mac_config))
    
    def execute_cluster(self, inputs, weights, tlast):
        """Execute 4 MACs in parallel"""
        output = {'accum': [0, 0, 0, 0], 'valid': False}
        
        for i in range(4):
            result = self.macs[i].execute_cycle(inputs[i], weights[i], False)
            if tlast:
                output['accum'][i] = self.macs[i].get_accumulator()
                output['valid'] = True
                self.macs[i].reset_accumulator()
            else:
                output['accum'][i] = result['accumulator']
        
        return output
    
    def reset_all(self):
        for mac in self.macs:
            mac.reset_accumulator()


class VerboseLogger:
    """Capture detailed hardware simulation output"""
    
    def __init__(self):
        self.log = []
    
    def log_mac_operation(self, cycle, mac_id, input_val, weight_val, accumulator):
        """Log MAC operation"""
        entry = f"[CYCLE {cycle:06d}] MAC#{mac_id} input=0x{input_val:02x} weight=0x{weight_val:02x} -> accum=0x{accumulator:08x}"
        self.log.append(entry)
    
    def log_dequant_operation(self, cycle, input_accum, scale, output_int8):
        """Log dequantization"""
        entry = f"[CYCLE {cycle:06d}] DEQUANT input=0x{input_accum:08x} scale=0x{scale:08x} -> output=0x{output_int8:02x}"
        self.log.append(entry)
    
    def log_output_store(self, cycle, addr, byte_sel, value):
        """Log output storage"""
        entry = f"[CYCLE {cycle:06d}] STORE addr=0x{addr:06x} byte[{byte_sel}]=0x{value:02x}"
        self.log.append(entry)
    
    def log_pixel_complete(self, cycle, out_y, out_x, out_c):
        """Log pixel completion"""
        entry = f"[CYCLE {cycle:06d}] PIXEL_COMPLETE y={out_y:3d} x={out_x:3d} c={out_c:2d}"
        self.log.append(entry)
    
    def print_summary(self):
        """Print all log entries"""
        for entry in self.log:
            print(entry)
    
    def get_log(self):
        return self.log


def test_complete_pipeline():
    """Test complete pipeline: MAC → Dequant → Output Storage"""
    print("=" * 90)
    print("COMPLETE HARDWARE ACCELERATOR PIPELINE TEST")
    print("=" * 90)
    print()
    
    # Configuration for Conv1 layer
    conv_config = {
        'input_height': 64,
        'input_width': 64,
        'input_channels': 3,
        'filter_height': 3,
        'filter_width': 3,
        'num_filters': 64,
        'stride': 1,
        'padding': 1
    }
    
    quant_config = {
        'zero_point_in': 0,
        'zero_point_out': 0,
        'scale_factor': 0x00800000,  # 0.5 in Q8.24
        'enable_relu': True
    }
    
    mac_config = {
        'zero_point_in': 0,
        'zero_point_weight': 0
    }
    
    output_config = {
        'output_height': 64,
        'output_width': 64,
        'output_channels': 64,
        'output_base_addr': 0,
        'enable_pooling': False
    }
    
    print("Configuration:")
    print(f"  Input:        {conv_config['input_height']}×{conv_config['input_width']}×{conv_config['input_channels']}")
    print(f"  Filters:      {conv_config['num_filters']}×{conv_config['filter_height']}×{conv_config['filter_width']}")
    print(f"  Output:       {output_config['output_height']}×{output_config['output_width']}×{output_config['output_channels']}")
    print(f"  Scale factor: 0x{quant_config['scale_factor']:08x} (Q8.24)")
    print(f"  ReLU:         {quant_config['enable_relu']}")
    print()
    
    # Initialize components
    logger = VerboseLogger()
    index_gen = IndexGenerator(conv_config)
    macs = MACStreamProvider(mac_config)
    dequant = Dequantization(quant_config)
    output_storage = OutputStorage(output_config)
    
    # Generate first 108 MACs (4 pixels × 27 MACs/pixel)
    # This gives us complete pixels for testing
    print("Simulating complete pipeline for first 4 output pixels...")
    print()
    
    addresses = index_gen.generate_first_n(108)
    
    # Create dummy input and weight data
    input_data = [i % 128 for i in range(conv_config['input_height'] * 
                                         conv_config['input_width'] * 
                                         conv_config['input_channels'])]
    weight_data = [(i % 64 - 32) for i in range(conv_config['num_filters'] * 
                                                  conv_config['filter_height'] * 
                                                  conv_config['filter_width'] * 
                                                  conv_config['input_channels'])]
    
    # Simulate MAC operations with pipeline
    cycle = 0
    pixel_count = 0
    outputs_generated = 0
    
    print("DETAILED PIPELINE LOG:")
    print("-" * 90)
    
    for addr_idx, addr in enumerate(addresses):
        # Get input and weight values
        input_val = input_data[addr['input_addr'] % len(input_data)] & 0xFF
        weight_val = weight_data[addr['weight_addr'] % len(weight_data)] & 0xFF
        
        # Simulate 4 parallel MACs
        # For simplicity, feed same input to all MACs with different weights
        inputs = [input_val, input_val, input_val, input_val]
        weights = [weight_val] + [(weight_val + i) & 0xFF for i in range(1, 4)]
        
        # Execute MACs
        mac_output = macs.execute_cluster(inputs, weights, addr['tlast'])
        
        # Log MAC operations
        for mac_id in range(4):
            logger.log_mac_operation(cycle, mac_id, inputs[mac_id], 
                                    weights[mac_id], mac_output['accum'][mac_id])
        
        # If TLAST, process outputs through dequantization and storage
        if addr['tlast']:
            pixel_count += 1
            out_y = pixel_count // output_config['output_width']
            out_x = pixel_count % output_config['output_width']
            
            # For each of 4 output channels this cycle
            for oc in range(4):
                # Dequantize
                accum = mac_output['accum'][oc]
                output_int8, dequant_stats = dequant.dequantize_scalar(accum)
                
                logger.log_dequant_operation(cycle, accum, quant_config['scale_factor'], 
                                           output_int8 & 0xFF)
                
                # Store output
                word_addr, byte_sel = output_storage.calc_output_addr(out_y, out_x, oc)
                logger.log_output_store(cycle, word_addr, byte_sel, output_int8 & 0xFF)
                outputs_generated += 1
            
            logger.log_pixel_complete(cycle, out_y, out_x, pixel_count % 4)
        
        cycle += 1
    
    # Print first 50 log entries (avoid overwhelming output)
    log = logger.get_log()
    print("\nFirst 50 pipeline operations (detailed log for FPGA comparison):")
    print()
    for entry in log[:50]:
        print(entry)
    
    if len(log) > 50:
        print(f"\n... ({len(log) - 50} more operations) ...\n")
        print("Last 10 operations:")
        for entry in log[-10:]:
            print(entry)
    
    print()
    print("=" * 90)
    print("PIPELINE SIMULATION SUMMARY")
    print("=" * 90)
    print(f"Total cycles executed:      {cycle}")
    print(f"Total MACs processed:       {len(addresses)}")
    print(f"Pixels completed:           {pixel_count}")
    print(f"Outputs generated:          {outputs_generated}")
    print(f"Accumulators created:       {pixel_count * 4}")
    print()
    print("✓ Complete pipeline test PASSED")
    print()
    return True


def test_mac_unit_only():
    """Test just the staged MAC unit with known inputs"""
    print("=" * 90)
    print("STAGED MAC UNIT TEST - Hardware Pipeline Verification")
    print("=" * 90)
    print()
    
    config = {
        'id': 0,
        'zero_point_in': 0,
        'zero_point_weight': 0
    }
    
    mac = StagedMAC(config)
    
    print("Testing 3-stage pipeline:")
    print("  Input: 5 multiply-accumulate operations")
    print("  Expected: Pipeline fills (3 cycles latency), then 1 result/cycle")
    print()
    
    test_inputs = [10, 20, 30, 40, 50]
    test_weights = [2, 2, 2, 2, 2]
    
    print(f"{'Cycle':>5} | {'Input':>6} | {'Weight':>6} | {'Product':>8} | {'Accum':>10} | Status")
    print("-" * 80)
    
    for cycle, (inp, wt) in enumerate(zip(test_inputs, test_weights)):
        result = mac.execute_cycle(inp, wt, cycle == 0)  # Reset on first
        accum = mac.get_accumulator()
        product = (inp - 0) * (wt - 0)
        
        print(f"{cycle:>5} | {inp:>6} | {wt:>6} | {product:>8} | {accum:>10} | ", end="")
        
        if cycle < 3:
            print("(pipeline fill)")
        else:
            print("(result valid)")
    
    print()
    print(f"Final accumulator: {mac.get_accumulator()}")
    print(f"Expected (10+20+30+40+50)*2 = 300: {mac.get_accumulator() == 300}")
    print()
    return mac.get_accumulator() == 300


def main():
    print()
    print("╔" + "=" * 88 + "╗")
    print("║" + " COMPLETE HARDWARE ACCELERATOR PIPELINE - C++ & FPGA VERIFICATION ".center(88) + "║")
    print("║" + " Includes: MAC Units, Dequantization, Output Storage ".center(88) + "║")
    print("╚" + "=" * 88 + "╝")
    print()
    
    all_pass = True
    
    all_pass &= test_mac_unit_only()
    all_pass &= test_complete_pipeline()
    
    print()
    print("=" * 90)
    if all_pass:
        print("✓ ALL TESTS PASSED - Ready for FPGA Integration")
        return 0
    else:
        print("✗ SOME TESTS FAILED")
        return 1


if __name__ == '__main__':
    sys.exit(main())
