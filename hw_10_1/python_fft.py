import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import glob
import os

# Create FFT directory if it doesn't exist
os.makedirs('FFT', exist_ok=True)

# Find all CSV files
csv_files = glob.glob('sig*.csv')

for csv_file in csv_files:
    # Load data
    data = pd.read_csv(csv_file, header=None)
    t = data[0].values
    y = data[1].values
    
    # Calculate sample rate from time column
    dt = t[1] - t[0]
    Fs = 1.0 / dt
    n = len(y)
    k = np.arange(n)
    T = n / Fs
    frq = k / T
    frq = frq[range(int(n/2))]
    Y = np.fft.fft(y) / n
    Y = Y[range(int(n/2))]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 6))
    fig.suptitle(f'{csv_file}')
    ax1.plot(t, y, 'b')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Amplitude')
    ax1.set_title('Signal vs Time')
    ax2.loglog(frq, np.abs(Y), 'b')
    ax2.set_xlabel('Freq (Hz)')
    ax2.set_ylabel('|Y(freq)|')
    ax2.set_title('FFT')
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    # Save the figure
    base = os.path.splitext(os.path.basename(csv_file))[0]
    plt.savefig(f'FFT/{base}.png')
    plt.close(fig)