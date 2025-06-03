# sudo apt-get install python3-pip
# python3 -m pip install pyserial
# sudo apt-get install python3-matplotlib


import matplotlib.pyplot as plt 

import serial
ser = serial.Serial('/dev/tty.usbmodem2101')
print('Opening port: ')
print(ser.name)

import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
reds = np.zeros((60, 80), dtype=np.uint8)
greens = np.zeros((60, 80), dtype=np.uint8)
blues = np.zeros((60, 80), dtype=np.uint8)

has_quit = False
# menu loop
while not has_quit:
    selection = input('\nENTER COMMAND: ')
    selection_endline = selection+'\n'
     
    # send the command 
    ser.write(selection_endline.encode()); # .encode() turns the string into a char array

    if (selection == 'c'):
        print("Waiting for camera data...")
        num_pixels = 4800
        # Read all data at once
        data = b''
        while data.count(b'\n') < num_pixels:
            data += ser.read(ser.in_waiting or 1)
        lines = data.split(b'\n')
        print(f"Received {len(lines)} lines of data.")
        for t, dat_str in enumerate(lines[:num_pixels]):
            try:
                value = int(dat_str.strip())
                row = t // 80
                col = t % 80
                reds[row][col] = value
                greens[row][col] = value
                blues[row][col] = value
            except ValueError:
                continue
        # Stack arrays to form an RGB image
        rgb_array = np.stack((reds, greens, blues), axis=-1)
        image = Image.fromarray(rgb_array)
        plt.imshow(image)
        plt.axis("off")
        plt.show()
    elif (selection == 'q'):
        print('Exiting client')
        has_quit = True; # exit client
        ser.close()
    else:
        print('Invalid Selection ' + selection_endline)
