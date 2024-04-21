import tkinter as tk
from tkinter import ttk
import requests
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

class HeartRateApp(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("Heart Rate and SpO2 Monitoring")

        # Styling
        self.configure(background="#f0f0f0")
        self.title_font = ("Helvetica", 18, "bold")

        # Container frame
        self.container = ttk.Frame(self, padding="20")
        self.container.grid(row=0, column=0, padx=20, pady=20, sticky="nsew")

        # SpO2 label and value
        self.spo2_label = ttk.Label(self.container, text="SpO2:", font=self.title_font)
        self.spo2_label.grid(row=0, column=0, padx=10, pady=10, sticky="w")
        self.spo2_value = ttk.Label(self.container, text="-", font=self.title_font)
        self.spo2_value.grid(row=0, column=1, padx=10, pady=10, sticky="w")

        # Heart Rate label and value
        self.heart_rate_label = ttk.Label(self.container, text="Heart Rate:", font=self.title_font)
        self.heart_rate_label.grid(row=1, column=0, padx=10, pady=10, sticky="w")
        self.heart_rate_value = ttk.Label(self.container, text="-", font=self.title_font)
        self.heart_rate_value.grid(row=1, column=1, padx=10, pady=10, sticky="w")

        # Graph frame
        self.graph_frame = ttk.Frame(self.container, padding="20")
        self.graph_frame.grid(row=2, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")

        # Initialize empty plot
        self.fig, self.ax = plt.subplots()
        self.plot, = self.ax.plot([], [])
        self.ax.set_xlabel('Time')
        self.ax.set_ylabel('Value')
        self.ax.legend(['Data'])
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.graph_frame)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # Start updating data and graph
        self.update_data()
        self.update_graph()

    def update_data(self):
        try:
            response = requests.get("http://192.168.147.131/data")  
            if response.status_code == 200:
                data = response.json()
                spo2_value = data["spo2"]
                heart_rate_value = data["beatAvg"]
                
                # Set text
                self.spo2_value.config(text=f"{spo2_value}%")
                self.heart_rate_value.config(text=f"{heart_rate_value} BPM")

                # Set color based on value
                self.spo2_value.configure(background=self.get_color_for_spo2(spo2_value))
                self.heart_rate_value.configure(background=self.get_color_for_bpm(heart_rate_value))
                
        except Exception as e:
            print("Error:", e)

        self.after(2000, self.update_data)  # Update data every 2 seconds

    def update_graph(self):
        try:
            response = requests.get("http://192.168.147.131/data")  # Replace with your ESP32 IP address
            if response.status_code == 200:
                data = response.json()
                beat_avg = data["beatAvg"]
                spo2 = data["spo2"]

                # Update plot
                self.plot.set_xdata(range(len([beat_avg, spo2])))
                self.plot.set_ydata([beat_avg, spo2])
                self.ax.relim()
                self.ax.autoscale_view(True, True, True)
                self.canvas.draw()

        except Exception as e:
            print("Error:", e)

        # Update graph every 2 seconds
        self.after(2000, self.update_graph)
    
    def get_color_for_spo2(self, spo2):
        if spo2 >= 95:
            return 'green'
        elif 90 <= spo2 < 95:
            return 'orange'
        else:
            return 'red'
    
    def get_color_for_bpm(self, bpm):
        if bpm >= 60 and bpm <= 100:
            return 'green'
        else:
            return 'red'

if __name__ == "__main__":
    app = HeartRateApp()
    app.mainloop()
