#!/usr/bin/env python3
"""
Dequantization Test - Reference Implementation Validation
"""

class Dequantization:
    """Python version of Dequantization for testing"""
    
    def __init__(self, config):
        self.config = config.copy()
    
    def set_quant_params(self, zero_point_in, zero_point_out, scale_factor):
        self.config['zero_point_in'] = zero_point_in
        self.config['zero_point_out'] = zero_point_out
        self.config['scale_factor'] = scale_factor
    
    def saturate_to_int8(self, value):
        if value > 127:
            return 127
        if value < -128:
            return -128
        return int(value) if isinstance(value, float) else value
    
    def dequantize_pipelined(self, accumulator):
        """Dequantize with all pipeline stages visible"""
        stats = {}
        
        # Stage 0: Input
        stats['accum_before'] = accumulator
        
        # Stage 1: Subtract zero-point
        stats['accum_after_zp'] = accumulator - self.config['zero_point_in']
        
        # Stage 2: Multiply by scale factor (Q8.24 fixed-point)
        # scale_factor is Q8.24, so we shift right by 24 after multiply
        temp = stats['accum_after_zp'] * self.config['scale_factor']
        
        # Add 0.5 in Q8.24 (0x00800000) before shifting for rounding
        stats['product'] = (temp + 0x00800000) >> 24
        
        # Stage 3: Round (already done)
        stats['rounded'] = stats['product']
        
        # Stage 4: Apply ReLU
        if self.config['enable_relu'] and stats['rounded'] < 0:
            stats['rounded'] = 0
        stats['after_relu'] = stats['rounded']
        
        # Stage 5: Add output zero-point and saturate to int8
        final_value = stats['after_relu'] + self.config['zero_point_out']
        stats['final'] = self.saturate_to_int8(final_value)
        
        return stats
    
    def dequantize_scalar(self, accumulator):
        """Dequantize single value"""
        stats = self.dequantize_pipelined(accumulator)
        return stats['final'], stats
    
    def dequantize_vector(self, accumulators):
        """Dequantize vector of values"""
        results = []
        all_stats = []
        
        for accum in accumulators:
            result, stats = self.dequantize_scalar(accum)
            results.append(result)
            all_stats.append(stats)
        
        return results, all_stats


def test_basic_dequantization():
    """Test basic dequantization with typical CNN parameters"""
    print("=" * 60)
    print("Dequantization Test - Basic Functionality")
    print("=" * 60)
    print()
    
    # Typical quantization parameters for CNN
    # Example: scale_factor = 0.5 in Q8.24 = 0x00800000
    # zero_point_in = 0 (activations already zero-centered)
    # zero_point_out = 0 (no output offset)
    config = {
        'zero_point_in': 0,
        'zero_point_out': 0,
        'scale_factor': 0x00800000,  # 0.5 in Q8.24
        'enable_relu': True,
        'enable_batch_norm': False
    }
    
    dequant = Dequantization(config)
    
    print("Configuration:")
    print(f"  Zero-point in:  {config['zero_point_in']}")
    print(f"  Zero-point out: {config['zero_point_out']}")
    print(f"  Scale factor:   0x{config['scale_factor']:08x} (Q8.24)")
    print(f"  Enable ReLU:    {config['enable_relu']}")
    print()
    
    # Test cases: accumulator -> expected output
    test_cases = [
        (0,      0,      "Zero input"),
        (100,    50,     "Positive (100 * 0.5 = 50)"),
        (200,    100,    "Large positive (200 * 0.5 = 100)"),
        (512,    127,    "Large positive (512 * 0.5 = 256, saturates to 127)"),
        (-100,   0,      "Negative with ReLU (-50 clipped to 0)"),
        (-50,    0,      "Negative with ReLU (-25 clipped to 0)"),
    ]
    
    print(f"{'Input':>8} | {'Expected':>8} | {'Output':>8} | {'Status':>6} | Description")
    print("-" * 70)
    
    passed = 0
    failed = 0
    
    for accum, expected, description in test_cases:
        result, stats = dequant.dequantize_scalar(accum)
        status = "✓ PASS" if result == expected else "✗ FAIL"
        
        if result == expected:
            passed += 1
        else:
            failed += 1
        
        print(f"{accum:>8} | {expected:>8} | {result:>8} | {status} | {description}")
    
    print()
    print(f"Results: {passed} passed, {failed} failed")
    print()
    
    return failed == 0


def test_saturation():
    """Test output saturation to int8 range"""
    print("=" * 60)
    print("Dequantization Test - Saturation")
    print("=" * 60)
    print()
    
    # Scale factor = 1.0 (no scaling)
    config = {
        'zero_point_in': 0,
        'zero_point_out': 0,
        'scale_factor': 0x01000000,  # 1.0 in Q8.24
        'enable_relu': False,  # Don't apply ReLU for saturation test
        'enable_batch_norm': False
    }
    
    dequant = Dequantization(config)
    
    print("Configuration:")
    print(f"  Scale factor:   0x{config['scale_factor']:08x} (Q8.24 = 1.0)")
    print(f"  Enable ReLU:    {config['enable_relu']}")
    print("  (Testing saturation boundaries)")
    print()
    
    # Test saturation boundaries
    test_cases = [
        (0,     0,      "Zero"),
        (127,   127,    "Max positive"),
        (128,   127,    "Overflow +1"),
        (255,   127,    "Large overflow"),
        (-128,  -128,   "Min negative"),
        (-129,  -128,   "Underflow -1"),
        (-200,  -128,   "Large underflow"),
    ]
    
    print(f"{'Input':>8} | {'Expected':>8} | {'Output':>8} | {'Status':>6} | Description")
    print("-" * 70)
    
    passed = 0
    failed = 0
    
    for accum, expected, description in test_cases:
        result, stats = dequant.dequantize_scalar(accum)
        status = "✓ PASS" if result == expected else "✗ FAIL"
        
        if result == expected:
            passed += 1
        else:
            failed += 1
        
        print(f"{accum:>8} | {expected:>8} | {result:>8} | {status} | {description}")
    
    print()
    print(f"Results: {passed} passed, {failed} failed")
    print()
    
    return failed == 0


def test_relu():
    """Test ReLU activation"""
    print("=" * 60)
    print("Dequantization Test - ReLU Activation")
    print("=" * 60)
    print()
    
    # Scale factor = 1.0 for simplicity
    config = {
        'zero_point_in': 0,
        'zero_point_out': 0,
        'scale_factor': 0x01000000,  # 1.0 in Q8.24
        'enable_relu': True,
        'enable_batch_norm': False
    }
    
    dequant = Dequantization(config)
    
    print("Configuration:")
    print(f"  Enable ReLU:    {config['enable_relu']}")
    print("  (Testing ReLU behavior)")
    print()
    
    test_cases = [
        (100,   100,    "Positive value passed through"),
        (50,    50,     "Positive value passed through"),
        (0,     0,      "Zero boundary"),
        (-1,    0,      "Negative clipped to 0"),
        (-50,   0,      "Large negative clipped to 0"),
        (-128,  0,      "Min negative clipped to 0"),
    ]
    
    print(f"{'Input':>8} | {'Expected':>8} | {'Output':>8} | {'Status':>6} | Description")
    print("-" * 70)
    
    passed = 0
    failed = 0
    
    for accum, expected, description in test_cases:
        result, stats = dequant.dequantize_scalar(accum)
        status = "✓ PASS" if result == expected else "✗ FAIL"
        
        if result == expected:
            passed += 1
        else:
            failed += 1
        
        print(f"{accum:>8} | {expected:>8} | {result:>8} | {status} | {description}")
    
    print()
    print(f"Results: {passed} passed, {failed} failed")
    print()
    
    return failed == 0


def test_vector_operations():
    """Test vector dequantization"""
    print("=" * 60)
    print("Dequantization Test - Vector Operations")
    print("=" * 60)
    print()
    
    config = {
        'zero_point_in': 0,
        'zero_point_out': 0,
        'scale_factor': 0x00800000,  # 0.5 in Q8.24
        'enable_relu': True,
        'enable_batch_norm': False
    }
    
    dequant = Dequantization(config)
    
    print("Configuration:")
    print(f"  Scale factor:   0x{config['scale_factor']:08x} (Q8.24 = 0.5)")
    print(f"  Enable ReLU:    {config['enable_relu']}")
    print()
    
    # Test vector
    accums = [0, 100, 200, -100, -50, 300]
    expected = [0, 50, 100, 0, 0, 127]  # 300*0.5=150, saturates to 127
    
    results, stats = dequant.dequantize_vector(accums)
    
    print("Vector dequantization test:")
    print(f"{'Index':>5} | {'Input':>8} | {'Expected':>8} | {'Output':>8} | {'Status':>6}")
    print("-" * 55)
    
    passed = 0
    failed = 0
    
    for i, (accum, exp, result) in enumerate(zip(accums, expected, results)):
        status = "✓ PASS" if result == exp else "✗ FAIL"
        if result == exp:
            passed += 1
        else:
            failed += 1
        
        print(f"{i:>5} | {accum:>8} | {exp:>8} | {result:>8} | {status}")
    
    print()
    print(f"Results: {passed} passed, {failed} failed")
    print(f"Vector length: {len(accums)}")
    print()
    
    return failed == 0


def main():
    import sys
    
    all_passed = True
    
    all_passed &= test_basic_dequantization()
    all_passed &= test_saturation()
    all_passed &= test_relu()
    all_passed &= test_vector_operations()
    
    print("=" * 60)
    if all_passed:
        print("✓ All Dequantization tests PASSED!")
        return 0
    else:
        print("✗ Some tests FAILED")
        return 1


if __name__ == '__main__':
    import sys
    sys.exit(main())
