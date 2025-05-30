import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy import signal
import glob
import os
import shutil

# Clear and recreate FIR directory
if os.path.exists('FIR'):
    shutil.rmtree('FIR')
os.makedirs('FIR')

# FIR Filter Parameters - Adjust these for each signal
# Based on FFT analysis of each signal
filter_params = {
    'sigA.csv': {
        'numtaps': 10000,        # Increased taps for potentially sharper cutoff
        'cutoff': 1,        # Keep cutoff at 1000Hz to pass main signals
        'width': 10,          # Transition width
        'window': 'hamming'    # Window type
    },
    'sigB.csv': {
        'numtaps': 5000,         # Slightly increased taps
        'cutoff': 1,         # Keep cutoff at 500Hz
        'width': 10,          # Transition width
        'window': 'hamming'
    },
    'sigC.csv': {
        'numtaps': 201,        # Increased taps for sharper cutoff at low frequency
        'cutoff': 10,          # Significantly lowered cutoff to 10Hz
        'width': 5,            # Narrower transition width
        'window': 'hamming'
    },
    'sigD.csv': {
        'numtaps': 3000,         # Slightly increased taps
        'cutoff': 1,          # Keep cutoff at 50Hz
        'width': 15,           # Transition width
        'window': 'hamming'
    }
}

def apply_fir_filter(signal_data, sample_rate, params):
    """Apply FIR filter to the signal data using the given parameters."""
    # Generate filter coefficients
    nyquist = sample_rate / 2
    cutoff_normalized = params['cutoff'] / nyquist
    width_normalized = params['width'] / nyquist
    
    # Create low-pass FIR filter
    coeffs = signal.firwin(
        numtaps=params['numtaps'],
        cutoff=cutoff_normalized,
        width=width_normalized,
        window=params['window']
    )
    
    # Apply filter
    filtered_signal = signal.lfilter(coeffs, 1, signal_data)
    
    return filtered_signal, coeffs

def plot_frequency_response(coeffs, sample_rate, params, csv_file):
    """Plot the frequency response of the filter."""
    w, h = signal.freqz(coeffs, worN=8000)
    plt.figure(figsize=(10, 6))
    plt.plot(0.5 * sample_rate * w / np.pi, np.abs(h), 'b')
    plt.title(f'Frequency Response - {csv_file}\n'
              f'Cutoff: {params["cutoff"]}Hz, '
              f'Taps: {params["numtaps"]}, '
              f'Window: {params["window"]}')
    plt.xlabel('Frequency [Hz]')
    plt.ylabel('Gain')
    plt.grid(True)
    base = os.path.splitext(os.path.basename(csv_file))[0]
    plt.savefig(f'FIR/{base}_freq_response.png')
    plt.close()

# Process each CSV file
csv_files = glob.glob('sig*.csv')

for csv_file in csv_files:
    # Load data
    data = pd.read_csv(csv_file, header=None)
    t = data[0].values
    y = data[1].values
    
    # Calculate sample rate
    dt = t[1] - t[0]
    sample_rate = 1.0 / dt
    
    # Get filter parameters for this file
    params = filter_params[csv_file]
    
    # Apply FIR filter
    y_fir, coeffs = apply_fir_filter(y, sample_rate, params)
    
    # Plot frequency response
    plot_frequency_response(coeffs, sample_rate, params, csv_file)
    
    # Plot original and filtered signals
    plt.figure(figsize=(12, 6))
    plt.plot(t, y, 'k', label='Original', alpha=0.5)
    plt.plot(t, y_fir, 'r', label='FIR Filtered')
    plt.xlabel('Time (s)')
    plt.ylabel('Amplitude')
    plt.title(f'{csv_file} - FIR Filter\n'
              f'Cutoff: {params["cutoff"]}Hz, '
              f'Taps: {params["numtaps"]}, '
              f'Window: {params["window"]}')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    
    # Save the figure
    base = os.path.splitext(os.path.basename(csv_file))[0]
    plt.savefig(f'FIR/{base}_filtered.png')
    plt.close() 