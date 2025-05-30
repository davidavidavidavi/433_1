import pandas as pd
import glob

# Get all CSV files
csv_files = glob.glob('sig*.csv')

for csv_file in csv_files:
    # Read the CSV file
    df = pd.read_csv(csv_file, header=None)
    
    # Calculate time duration (last time - first time)
    duration = df[0].iloc[-1] - df[0].iloc[0]
    
    # Calculate number of samples
    num_samples = len(df)
    
    # Calculate sample rate (samples per second)
    sample_rate = num_samples / duration
    
    print(f"\nFile: {csv_file}")
    print(f"Duration: {duration:.3f} seconds")
    print(f"Number of samples: {num_samples}")
    print(f"Sample rate: {sample_rate:.1f} Hz") 