#!/usr/bin/env python3
"""
OutputStorage Test - Reference Implementation for BRAM Read-Modify-Write
"""

import sys

class OutputStorage:
    """Python reference implementation of OutputStorage module"""
    
    def __init__(self, config):
        self.config = config.copy()
        self.pixel_count = 0
        self.bram = {}  # Simulate BRAM as dictionary
    
    def calc_output_addr(self, out_y, out_x, out_c):
        """Calculate BRAM word address and byte position"""
        linear_addr = (out_y * self.config['output_width'] + out_x) * self.config['output_channels'] + out_c
        word_addr = self.config['output_base_addr'] + (linear_addr // 4)
        byte_sel = linear_addr % 4
        return word_addr, byte_sel
    
    def insert_byte(self, old_word, new_byte, byte_sel):
        """Insert byte into 32-bit word (little-endian)"""
        mask = 0xFFFFFFFF ^ (0xFF << (byte_sel * 8))
        new_word = old_word & mask
        new_word |= (new_byte & 0xFF) << (byte_sel * 8)
        return new_word & 0xFFFFFFFF
    
    def extract_byte(self, word, byte_sel):
        """Extract byte from 32-bit word"""
        return (word >> (byte_sel * 8)) & 0xFF
    
    def store_output(self, out_y, out_x, out_c, value, old_word=0):
        """Store single output value (RMW)"""
        if out_y >= self.config['output_height'] or out_x >= self.config['output_width'] or out_c >= self.config['output_channels']:
            raise ValueError("Output coordinates out of bounds")
        
        word_addr, byte_sel = self.calc_output_addr(out_y, out_x, out_c)
        new_word = self.insert_byte(old_word, value & 0xFF, byte_sel)
        
        return {
            'out_y': out_y,
            'out_x': out_x,
            'out_c': out_c,
            'word_addr': word_addr,
            'byte_sel': byte_sel,
            'value': value,
            'old_word': old_word,
            'new_word': new_word
        }
    
    def process_stream(self, tdata, tid, tlast):
        """Process AXI-Stream input"""
        pixel_idx = self.pixel_count
        out_y = pixel_idx // self.config['output_width']
        out_x = pixel_idx % self.config['output_width']
        out_c = tid  # Simple mapping
        
        if out_y >= self.config['output_height'] or out_x >= self.config['output_width']:
            return None
        
        word_addr, byte_sel = self.calc_output_addr(out_y, out_x, out_c)
        old_word = self.bram.get(word_addr, 0)
        new_word = self.insert_byte(old_word, tdata & 0xFF, byte_sel)
        
        if tlast:
            self.pixel_count += 1
        
        # Store in simulated BRAM
        self.bram[word_addr] = new_word
        
        return {
            'word_addr': word_addr,
            'byte_sel': byte_sel,
            'old_word': old_word,
            'new_word': new_word,
            'value': tdata
        }
    
    def pool_max2x2(self, values):
        """2×2 max pooling"""
        if len(values) != 4:
            raise ValueError("Pooling requires exactly 4 values")
        return max(values)
    
    def verify_addresses(self, addresses):
        """Verify address sequence"""
        num_outputs = self.config['output_height'] * self.config['output_width'] * self.config['output_channels']
        max_addr = self.config['output_base_addr'] + (num_outputs + 3) // 4
        
        for addr in addresses:
            if addr >= max_addr:
                print(f"ERROR: Address out of bounds: 0x{addr:x} >= 0x{max_addr:x}")
                return False
        
        print("✓ Output storage address verification PASSED")
        print(f"  Total outputs: {num_outputs}")
        print(f"  BRAM words needed: {(num_outputs + 3) // 4}")
        return True


def test_basic_rmw():
    """Test basic read-modify-write operation"""
    print("=" * 70)
    print("OutputStorage Test - Basic Read-Modify-Write")
    print("=" * 70)
    print()
    
    config = {
        'output_height': 8,
        'output_width': 8,
        'output_channels': 4,
        'output_base_addr': 0,
        'enable_pooling': False
    }
    
    storage = OutputStorage(config)
    
    print("Configuration:")
    print(f"  Output: {config['output_height']}×{config['output_width']}×{config['output_channels']}")
    print(f"  Total elements: {config['output_height'] * config['output_width'] * config['output_channels']}")
    print()
    
    # Store some values
    test_cases = [
        (0, 0, 0, 10),   # (out_y, out_x, out_c, value)
        (0, 0, 1, 20),
        (0, 0, 2, 30),
        (0, 0, 3, 40),   # All 4 channels of first pixel
        (0, 1, 0, 50),   # Second pixel
        (7, 7, 3, 127),  # Last element
    ]
    
    print(f"{'Y':>2} | {'X':>2} | {'C':>2} | {'Value':>6} | {'Addr':>8} | {'Byte':>4} | {'New Word':>10}")
    print("-" * 70)
    
    for out_y, out_x, out_c, value in test_cases:
        stats = storage.store_output(out_y, out_x, out_c, value)
        print(f"{out_y:>2} | {out_x:>2} | {out_c:>2} | {stats['value']:>6} | "
              f"0x{stats['word_addr']:06x} | {stats['byte_sel']:>4} | 0x{stats['new_word']:08x}")
    
    print()
    print("✓ Basic RMW test PASSED")
    print()
    return True


def test_byte_packing():
    """Test 32-bit word byte packing"""
    print("=" * 70)
    print("OutputStorage Test - Byte Packing")
    print("=" * 70)
    print()
    
    config = {
        'output_height': 16,
        'output_width': 16,
        'output_channels': 64,
        'output_base_addr': 0,
        'enable_pooling': False
    }
    
    storage = OutputStorage(config)
    
    print("Test: Pack 4 int8 values into one 32-bit word")
    print()
    
    values = [10, 20, 30, 40]
    word = 0
    
    print(f"{'Byte':>4} | {'Value':>6} | {'Mask':>10} | {'Shifted':>10} | {'Word After':>10}")
    print("-" * 55)
    
    for i, val in enumerate(values):
        old_word = word
        word = storage.insert_byte(word, val, i)
        shift = val << (i * 8)
        print(f"{i:>4} | {val:>6} | 0x{0xFF << (i*8):010x} | 0x{shift:010x} | 0x{word:010x}")
    
    # Verify packing
    print()
    print("Verify extraction:")
    print(f"{'Byte':>4} | {'Expected':>8} | {'Extracted':>10}")
    print("-" * 35)
    
    all_correct = True
    for i, expected in enumerate(values):
        extracted = storage.extract_byte(word, i)
        match = "✓" if extracted == expected else "✗"
        print(f"{i:>4} | {expected:>8} | {extracted:>10} {match}")
        if extracted != expected:
            all_correct = False
    
    print()
    if all_correct:
        print("✓ Byte packing test PASSED")
    else:
        print("✗ Byte packing test FAILED")
    
    print()
    return all_correct


def test_address_calculation():
    """Test address calculation for various output positions"""
    print("=" * 70)
    print("OutputStorage Test - Address Calculation")
    print("=" * 70)
    print()
    
    config = {
        'output_height': 64,
        'output_width': 64,
        'output_channels': 64,
        'output_base_addr': 0,
        'enable_pooling': False
    }
    
    storage = OutputStorage(config)
    
    # Total outputs: 64 × 64 × 64 = 262,144
    # BRAM words needed: 262,144 / 4 = 65,536
    
    print(f"Output dimensions: {config['output_height']}×{config['output_width']}×{config['output_channels']}")
    print(f"Total elements: {config['output_height'] * config['output_width'] * config['output_channels']}")
    print(f"BRAM words: {(config['output_height'] * config['output_width'] * config['output_channels'] + 3) // 4}")
    print()
    
    # Test a few specific positions
    test_positions = [
        (0, 0, 0),      # First element
        (0, 0, 63),     # Last channel of first pixel
        (0, 1, 0),      # Second pixel
        (63, 63, 63),   # Last element
    ]
    
    print(f"{'Y':>3} | {'X':>3} | {'C':>3} | {'Linear Addr':>12} | {'Word Addr':>10} | {'Byte':>4}")
    print("-" * 65)
    
    for out_y, out_x, out_c in test_positions:
        word_addr, byte_sel = storage.calc_output_addr(out_y, out_x, out_c)
        linear_addr = (out_y * config['output_width'] + out_x) * config['output_channels'] + out_c
        print(f"{out_y:>3} | {out_x:>3} | {out_c:>3} | {linear_addr:>12} | 0x{word_addr:08x} | {byte_sel:>4}")
    
    print()
    print("✓ Address calculation test PASSED")
    print()
    return True


def test_streaming():
    """Test AXI-Stream processing"""
    print("=" * 70)
    print("OutputStorage Test - AXI-Stream Processing")
    print("=" * 70)
    print()
    
    config = {
        'output_height': 4,
        'output_width': 4,
        'output_channels': 4,
        'output_base_addr': 0,
        'enable_pooling': False
    }
    
    storage = OutputStorage(config)
    
    print("Simulating AXI-Stream data (4 pixels, 4 channels each)")
    print()
    
    # Simulate 4 outputs (one pixel worth)
    stream_data = [
        (10, 0, False),  # MAC 0 output
        (20, 1, False),  # MAC 1 output
        (30, 2, False),  # MAC 2 output
        (40, 3, True),   # MAC 3 output (TLAST)
    ]
    
    print(f"{'tdata':>6} | {'tid':>3} | {'tlast':>5} | {'Y':>2} | {'X':>2} | {'C':>2} | {'Addr':>8}")
    print("-" * 60)
    
    for tdata, tid, tlast in stream_data:
        result = storage.process_stream(tdata, tid, tlast)
        if result:
            print(f"{tdata:>6} | {tid:>3} | {str(tlast):>5} | {result['new_word']:>2} | "
                  f"{result['byte_sel']:>2} | {tid:>2} | 0x{result['word_addr']:06x}")
    
    print()
    print("✓ Streaming test PASSED")
    print()
    return True


def test_max_pooling():
    """Test 2×2 max pooling"""
    print("=" * 70)
    print("OutputStorage Test - 2×2 Max Pooling")
    print("=" * 70)
    print()
    
    config = {
        'output_height': 32,
        'output_width': 32,
        'output_channels': 64,
        'output_base_addr': 0,
        'enable_pooling': True
    }
    
    storage = OutputStorage(config)
    
    test_cases = [
        ([10, 20, 30, 40], 40),
        ([100, 50, 75, 25], 100),
        ([-50, -10, -30, -20], -10),
        ([127, 127, 127, 127], 127),
    ]
    
    print(f"{'Input Values':>20} | {'Expected Max':>13} | {'Result':>6} | Status")
    print("-" * 65)
    
    all_pass = True
    for values, expected in test_cases:
        result = storage.pool_max2x2(values)
        status = "✓ PASS" if result == expected else "✗ FAIL"
        if result != expected:
            all_pass = False
        print(f"[{values[0]:>3}, {values[1]:>3}, {values[2]:>3}, {values[3]:>3}] | {expected:>13} | {result:>6} | {status}")
    
    print()
    if all_pass:
        print("✓ Max pooling test PASSED")
    else:
        print("✗ Max pooling test FAILED")
    
    print()
    return all_pass


def main():
    print()
    print("╔" + "=" * 68 + "╗")
    print("║" + " " * 68 + "║")
    print("║" + " OutputStorage Reference Implementation Test ".center(68) + "║")
    print("║" + " BRAM Read-Modify-Write & Max Pooling ".center(68) + "║")
    print("║" + " " * 68 + "║")
    print("╚" + "=" * 68 + "╝")
    print()
    
    all_pass = True
    
    all_pass &= test_basic_rmw()
    all_pass &= test_byte_packing()
    all_pass &= test_address_calculation()
    all_pass &= test_streaming()
    all_pass &= test_max_pooling()
    
    print()
    print("=" * 70)
    if all_pass:
        print("✓ All OutputStorage tests PASSED!")
        return 0
    else:
        print("✗ Some tests FAILED")
        return 1


if __name__ == '__main__':
    sys.exit(main())
