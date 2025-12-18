
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models
import struct
import os

def load_weights(layer, weight_path, bias_path=None):
    """
    Load weights from binary files and set them to the Keras layer.
    Assumes binary files are simple flat float32 arrays.
    Handles reshaping for Conv2D (H,W,I,O) and Dense (I,O).
    """
    if not os.path.exists(weight_path):
        print(f"Error: Weight file not found: {weight_path}")
        return
    
    with open(weight_path, 'rb') as f:
        weight_data = f.read()
    
    # Calculate expected number of elements
    num_elements = len(weight_data) // 4
    weights = struct.unpack(f'{num_elements}f', weight_data)
    weights = np.array(weights, dtype=np.float32)
    
    # Reshape based on layer type and its expected shape
    # Keras Conv2D kernels are [filter_height, filter_width, in_channels, out_channels]
    # Keras Dense kernels are [input_dim, output_dim]
    expected_shape = layer.kernel.shape
    try:
        if isinstance(layer, layers.Conv2D):
             # Try standard layout first
            weights_reshaped = weights.reshape(expected_shape)
        elif isinstance(layer, layers.Dense):
            # Dense layers often store weights as [Out, In] in binary dumps for efficiency
            # Keras expects [In, Out]. So we reshape to [Out, In] and transpose.
            # expected_shape is (In, Out)
            weights_reshaped = weights.reshape((expected_shape[1], expected_shape[0])).T
        else:
             weights_reshaped = weights # Should not happen with just Conv/Dense
    except ValueError:
         print(f"Error reshaping weights for layer {layer.name}. Expected {expected_shape}, got flat size {num_elements}")
         return


    if bias_path and os.path.exists(bias_path):
        with open(bias_path, 'rb') as f:
            bias_data = f.read()
        num_bias = len(bias_data) // 4
        biases = struct.unpack(f'{num_bias}f', bias_data)
        biases = np.array(biases, dtype=np.float32)
        layer.set_weights([weights_reshaped, biases])
    else:
        layer.set_weights([weights_reshaped])
        
    print(f"Loaded weights for {layer.name} from {weight_path}")

def build_and_load_model(weights_dir):
    # Define Model Architecture matching ML.cpp
    # Input: 64x64x3
    
    input_layer = layers.Input(shape=(64, 64, 3))

    # Conv 1: 5x5, 32 filters
    x = layers.Conv2D(32, (5, 5), padding='valid', activation='relu', name='conv1')(input_layer)
    # Output: 60x60x32

    # Conv 2: 5x5, 32 filters
    x = layers.Conv2D(32, (5, 5), padding='valid', activation='relu', name='conv2')(x)
    # Output: 56x56x32
    
    # MaxPool 1: 2x2
    x = layers.MaxPooling2D((2, 2), strides=(2, 2), padding='valid', name='pool1')(x)
    # Output: 28x28x32
    
    # Conv 3: 3x3, 64 filters
    x = layers.Conv2D(64, (3, 3), padding='valid', activation='relu', name='conv3')(x)
    # Output: 26x26x64

    # Conv 4: 3x3, 64 filters
    x = layers.Conv2D(64, (3, 3), padding='valid', activation='relu', name='conv4')(x)
    # Output: 24x24x64

    # MaxPool 2: 2x2
    x = layers.MaxPooling2D((2, 2), strides=(2, 2), padding='valid', name='pool2')(x)
    # Output: 12x12x64
    
    # Conv 5: 3x3, 64 filters
    x = layers.Conv2D(64, (3, 3), padding='valid', activation='relu', name='conv5')(x)
    # Output: 10x10x64

    # Conv 6: 3x3, 128 filters
    x = layers.Conv2D(128, (3, 3), padding='valid', activation='relu', name='conv6')(x)
    # Output: 8x8x128
    
    # MaxPool 3: 2x2
    x = layers.MaxPooling2D((2, 2), strides=(2, 2), padding='valid', name='pool3')(x)
    # Output: 4x4x128
    
    # Flatten
    x = layers.Flatten(name='flatten')(x)
    # Output: 2048

    # Dense 1: 256
    x = layers.Dense(256, activation='relu', name='dense1')(x)
    
    # Model up to Dense 1 (Layer 11) for intermediate output
    model_dense1 = models.Model(inputs=input_layer, outputs=x)

    # Dense 2: 200
    x = layers.Dense(200, name='dense2')(x)

    # Softmax
    outputs = layers.Softmax(name='softmax')(x)
    
    # Full Model
    model = models.Model(inputs=input_layer, outputs=outputs)
    
    # Load Weights
    load_weights(model.get_layer('conv1'), os.path.join(weights_dir, 'conv1_weights.bin'), os.path.join(weights_dir, 'conv1_biases.bin'))
    load_weights(model.get_layer('conv2'), os.path.join(weights_dir, 'conv2_weights.bin'), os.path.join(weights_dir, 'conv2_biases.bin'))
    load_weights(model.get_layer('conv3'), os.path.join(weights_dir, 'conv3_weights.bin'), os.path.join(weights_dir, 'conv3_biases.bin'))
    load_weights(model.get_layer('conv4'), os.path.join(weights_dir, 'conv4_weights.bin'), os.path.join(weights_dir, 'conv4_biases.bin'))
    load_weights(model.get_layer('conv5'), os.path.join(weights_dir, 'conv5_weights.bin'), os.path.join(weights_dir, 'conv5_biases.bin'))
    load_weights(model.get_layer('conv6'), os.path.join(weights_dir, 'conv6_weights.bin'), os.path.join(weights_dir, 'conv6_biases.bin'))
    load_weights(model.get_layer('dense1'), os.path.join(weights_dir, 'dense1_weights.bin'), os.path.join(weights_dir, 'dense1_biases.bin'))
    load_weights(model.get_layer('dense2'), os.path.join(weights_dir, 'dense2_weights.bin'), os.path.join(weights_dir, 'dense2_biases.bin'))
    
    return model, model_dense1

def run_inference(image_path, weights_dir, output_dir):
    print("Building model...")
    model, model_dense1 = build_and_load_model(weights_dir)
    
    print(f"Loading image from {image_path}...")
    with open(image_path, 'rb') as f:
        img_data = f.read()
    
    img_floats = struct.unpack(f'{len(img_data)//4}f', img_data)
    img_array = np.array(img_floats, dtype=np.float32).reshape(1, 64, 64, 3)
    
    print("Running inference...")
    
    # Layer 11 Output (Dense 1 output)
    dense1_output = model_dense1.predict(img_array)
    dense1_output_flat = dense1_output.flatten()
    
    # Check consistency if old file exists
    # Layer 10 (Dense 1) corresponds to index 10 in C++ (outputs 256)
    dense1_output_flat = dense1_output.flatten()
    old_l10_path = os.path.join(output_dir, "layer_10_output.bin")
    if os.path.exists(old_l10_path):
        print(f"Checking Layer 10 (Dense 1)...")
        with open(old_l10_path, 'rb') as f:
            old_data = struct.unpack(f'{os.path.getsize(old_l10_path)//4}f', f.read())
        old_arr = np.array(old_data, dtype=np.float32)
        if old_arr.shape == dense1_output_flat.shape:
             print("Layer 10 Shapes match.")
        else:
             print(f"Layer 10 Shape mismatch: Old {old_arr.shape}, New {dense1_output_flat.shape}")

    # Layer 11 (Dense 2) (outputs 200)
    # We need to get intermediate output for Dense 2
    model_dense2 = models.Model(inputs=model.input, outputs=model.get_layer('dense2').output)
    dense2_output = model_dense2.predict(img_array)
    dense2_output_flat = dense2_output.flatten()
    
    l11_save_path = os.path.join(output_dir, "layer_11_output_regen.bin")
    with open(l11_save_path, 'wb') as f:
        f.write(struct.pack(f'{len(dense2_output_flat)}f', *dense2_output_flat))
    print(f"Saved regenerated Layer 11 (Dense 2) output to {l11_save_path}")

    # Layer 10 Output
    l10_save_path = os.path.join(output_dir, "layer_10_output_regen.bin")
    with open(l10_save_path, 'wb') as f:
        f.write(struct.pack(f'{len(dense1_output_flat)}f', *dense1_output_flat))
    print(f"Saved regenerated Layer 10 (Dense 1) output to {l10_save_path}")

    # Layer 12 Output (Softmax)
    final_output = model.predict(img_array)
    final_output_flat = final_output.flatten()
    
    l12_save_path = os.path.join(output_dir, "layer_12_output_regen.bin")
    with open(l12_save_path, 'wb') as f:
        f.write(struct.pack(f'{len(final_output_flat)}f', *final_output_flat))
    print(f"Saved regenerated Layer 12 (Softmax) output to {l12_save_path}")

if __name__ == "__main__":
    BASE_DIR = "/Users/shiv/Downloads/lab3_src_14/SW/sw_quant_framework/data"
    WEIGHTS_DIR = os.path.join(BASE_DIR, "model")
    IMAGE_PATH = os.path.join(BASE_DIR, "image_0.bin")
    OUTPUT_DIR = os.path.join(BASE_DIR, "image_0_data")
    
    run_inference(IMAGE_PATH, WEIGHTS_DIR, OUTPUT_DIR)
