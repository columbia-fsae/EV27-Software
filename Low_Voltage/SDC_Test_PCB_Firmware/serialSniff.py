# python GUI.py
# Main control file: read serial from arduino and save to file + send inputs to arduino to determine what tests to run

import serial
import threading
import tkinter as tk
from tkinter import filedialog
import time
import datetime

class SerialGUI:

    # UI
    def __init__(self, root):
        self.root = root
        self.root.title("Arduino Serial Monitor")

        self.ser = None
        self.running = False
        self.log_file = None

        # ----- Serial Settings -----

        tk.Label(root, text="COM Port").grid(row=0, column=0)
        self.port_entry = tk.Entry(root)
        self.port_entry.insert(0, "COM3")
        self.port_entry.grid(row=0, column=1)

        tk.Label(root, text="Baud").grid(row=0, column=2)
        self.baud_entry = tk.Entry(root)
        self.baud_entry.insert(0, "9600")
        self.baud_entry.grid(row=0, column=3)

        # ----- Serial Display -----

        self.text_area = tk.Text(root, height=20, width=80)
        self.text_area.grid(row=1, column=0, columnspan=4)

        # ----- Send Box -----

        tk.Label(root, text="Send").grid(row=2, column=0)

        self.send_entry = tk.Entry(root, width=50)
        self.send_entry.grid(row=2, column=1, columnspan=2)

        self.send_button = tk.Button(root, text="Send",
                                     command=self.send_serial)
        self.send_button.grid(row=2, column=3)

        # ----- File Location -----

        tk.Label(root, text="Save File").grid(row=3, column=0)

        self.file_entry = tk.Entry(root, width=50)
        self.file_entry.grid(row=3, column=1, columnspan=2)

        self.browse_button = tk.Button(root,
                                       text="Browse",
                                       command=self.browse_file)
        self.browse_button.grid(row=3, column=3)

        # ----- Control Buttons -----

        self.start_button = tk.Button(root,
                                      text="Start",
                                      command=self.start)
        self.start_button.grid(row=4, column=0)

        self.stop_button = tk.Button(root,
                                     text="Stop",
                                     command=self.stop)
        self.stop_button.grid(row=4, column=1)

        self.clear_button = tk.Button(root,
                                      text="Clear",
                                      command=self.clear_text)
        self.clear_button.grid(row=4, column=2)


    # ----- File Browser -----

    def browse_file(self):

        filename = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("Text Files", "*.txt")]
        )

        if filename:
            self.file_entry.delete(0, tk.END)
            self.file_entry.insert(0, filename)


    # ----- Serial Reading Thread ----- (read from arduino outputs)
    def read_serial(self):

        while self.running:

            try:

                if self.ser.in_waiting:

                    line = self.ser.readline().decode(errors='ignore')

                    self.text_area.insert(tk.END, line)
                    self.text_area.see(tk.END)

                    if self.log_file:
                        self.log_file.write(line)
                        self.log_file.flush()

            except:
                pass


    # ----- Start Serial -----
    def start(self):

        port = self.port_entry.get()
        baud = int(self.baud_entry.get())
        filename = self.file_entry.get()

        try:

            self.ser = serial.Serial(port, baud)
            time.sleep(2)

            if filename:
                self.log_file = open(filename, "a")
                self.log_file.write(datetime.datetime.now().strftime("%m-%d-%Y %H:%M:%S"))
                self.log_file.write("\n\n")
                self.log_file.flush()

            self.running = True

            self.thread = threading.Thread(target=self.read_serial)
            self.thread.daemon = True
            self.thread.start()

        except Exception as e:
            print("Error:", e)


    # ----- Stop -----

    def stop(self):

        self.running = False

        if self.ser:
            self.ser.close()

        if self.log_file:
            self.log_file.close()


    # ----- Send Serial -----

    def send_serial(self):

        if self.ser:

            msg = self.send_entry.get()

            self.ser.write((msg + "\n").encode())
            self.ser.flush()

            self.send_entry.delete(0, tk.END)


    # ----- Clear Display -----

    def clear_text(self):

        self.text_area.delete(1.0, tk.END)


# ----- Main -----

root = tk.Tk()

# options = tk.Label(root, text=
# "m: BMS Unit Test" \
# "\np: BPT Unit Test" \
# "\nc: CT Unit Test" \
# "\nd: Dual Unit Test" \
# "\ni: IMD Unit Test" \
# "\nn: N Cycle Unit Test (only after running all unit tests)")
# options.grid(row=1, column=1)

app = SerialGUI(root)

print("Set a filename to save output data!")
print("m: BMS Unit Test" \
"\np: BPT Unit Test" \
"\nc: CT Unit Test" \
"\nd: Dual Unit Test" \
"\ni: IMD Unit Test" \
"\nn: N Cycle Unit Test (only after running all unit tests)") # instructions for what characters trigger what tests via the arduino
root.mainloop()