import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import glob
import os

# Create MAF directory if it doesn't exist
os.makedirs('MAF', exist_ok=True)

# Aggressive X values for moving average
X_values = [100, 500, 1000, 5000]

# Find all CSV files
csv_files = glob.glob('sig*.csv')

for csv_file in csv_files:
    # Load data
    data = pd.read_csv(csv_file, header=None)
    t = data[0].values
    y = data[1].values

    for X in X_values:
        # Moving average filter (aggressive low-pass)
        # Pad with zeros at the start
        y_padded = np.concatenate((np.zeros(X-1), y))
        y_maf = np.convolve(y_padded, np.ones(X)/X, mode='valid')
        
        plt.figure(figsize=(12, 6))
        plt.plot(t, y, 'k', label='Original', alpha=0.5)
        plt.plot(t, y_maf, 'r', label=f'MAF (X={X})')
        plt.xlabel('Time (s)')
        plt.ylabel('Amplitude')
        plt.title(f'{csv_file} - Moving Average Filter (X={X})')
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        
        # Save the figure
        base = os.path.splitext(os.path.basename(csv_file))[0]
        plt.savefig(f'MAF/{base}_MAF_X{X}.png')
        plt.close() 