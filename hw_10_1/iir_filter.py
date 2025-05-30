import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import glob
import os
import shutil

# Clear and recreate IIR directory
if os.path.exists('IIR'):
    shutil.rmtree('IIR')
os.makedirs('IIR')

# Define weight combinations for different signals
weight_combinations = {
    'sigA.csv': [  # Most aggressive for signal A
        (0.995, 0.005),  # Nearly total emphasis on previous value
        (0.998, 0.002),  # Almost total emphasis on previous value
        (0.999, 0.001),  # Extremely strong emphasis on previous value
        (0.9995, 0.0005) # Nearly complete emphasis on previous value
    ],
    'sigB.csv': [  # Keep current aggressive settings for signal B
        (0.95, 0.05),
        (0.98, 0.02),
        (0.99, 0.01),
        (0.995, 0.005)
    ],
    'sigC.csv': [  # Less aggressive for signal C
        (0.7, 0.3),    # Moderate emphasis on previous value
        (0.8, 0.2),    # More emphasis on previous value
        (0.9, 0.1),    # Strong emphasis on previous value
        (0.95, 0.05)   # Very strong emphasis on previous value
    ],
    'sigD.csv': [  # Keep current aggressive settings for signal D
        (0.95, 0.05),
        (0.98, 0.02),
        (0.99, 0.01),
        (0.995, 0.005)
    ]
}

# Find all CSV files
csv_files = glob.glob('sig*.csv')

for csv_file in csv_files:
    # Load data
    data = pd.read_csv(csv_file, header=None)
    t = data[0].values
    y = data[1].values

    # Get weights for this specific file
    weights = weight_combinations[csv_file]

    for A, B in weights:
        # Initialize filtered signal
        y_iir = np.zeros_like(y)
        y_iir[0] = y[0]  # First value is same as original
        
        # Apply IIR filter
        for i in range(1, len(y)):
            y_iir[i] = A * y_iir[i-1] + B * y[i]
        
        plt.figure(figsize=(12, 6))
        plt.plot(t, y, 'k', label='Original', alpha=0.5)
        plt.plot(t, y_iir, 'r', label=f'IIR (A={A}, B={B})')
        plt.xlabel('Time (s)')
        plt.ylabel('Amplitude')
        plt.title(f'{csv_file} - IIR Filter (A={A}, B={B})')
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        
        # Save the figure
        base = os.path.splitext(os.path.basename(csv_file))[0]
        plt.savefig(f'IIR/{base}_IIR_A{A}_B{B}.png')
        plt.close() 