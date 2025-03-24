import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
import paho.mqtt.client as mqtt
import threading
import datetime
import mysql.connector

# MQTT Configuration
MQTT_BROKER = "broker.emqx.io"
MQTT_PORT = 1883
MQTT_TOPIC_COMMANDS = "Test/commands"
MQTT_TOPIC_BMW520d = "BMW/520d"
MQTT_TOPIC_BMW530d = "BMW/530d"
MQTT_TOPIC_RECEIVE = "Test/status"
MQTT_TOPIC_CREATE = ""

# MySQL Configuration
DB_CONFIG = {
    "host": "localhost",
    "user": "Auto",
    "password": "Auto",
    "database": "cars"
}

# Initialize MySQL connection
def connect_db():
    return mysql.connector.connect(**DB_CONFIG)


# Fetch vehicles from MySQL
def fetch_vehicles():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute("SELECT vin, name, details FROM carsandvin")
    vehicles = {f"{name} ({vin})": vin for vin, name, details in cursor.fetchall()}
    conn.close()
    return vehicles


# Fetch blob files from the second MySQL table
def fetch_files():
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute("SELECT id, version, carname FROM filesandcars WHERE file IS NOT NULL")
    files = [f"{carname} (Version {version}) - File ID: {file_id}" for file_id, version, carname in cursor.fetchall()]
    conn.close()
    return files


# Insert message into MySQL
def log_message(msg):
    conn = connect_db()
    cursor = conn.cursor()
    cursor.execute("INSERT INTO messages (message) VALUES (%s)", (msg,))
    conn.commit()
    conn.close()

# Initialize MQTT client
client = mqtt.Client(protocol=mqtt.MQTTv311)

# Load vehicles and files from MySQL
vehicles = fetch_vehicles()
files = fetch_files()

# Commands for dropdown
commands = [
    "21 - Prepare transfer",
    "22 - Transfer Data",
    "23 - Finish transfer",
    "24 - Abort"
]

# Function to handle incoming MQTT messages
def on_message(client, userdata, msg):
    timestamp = datetime.datetime.now().strftime("[%H:%M:%S]")
    received_message = f"{timestamp} Master state: {msg.payload.decode()}"
    root.after(0, lambda: update_gui(received_message))

def update_gui(message):
    message_display.insert(tk.END, message + "\n")
    message_display.see(tk.END)

# Function to start MQTT subscription
def mqtt_subscribe():
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC_RECEIVE)
    client.loop_forever()


def start_mqtt_thread():
    thread = threading.Thread(target=mqtt_subscribe, daemon=True)
    thread.start()


def activate_vin():
    """ Retrieves the VIN from the database based on the selected vehicle,
        sends it via MQTT to the command topic, and creates a new topic by subscribing to it """
    global MQTT_TOPIC_CREATE

    selected_vehicle = vehicle_var.get()  # Get selected vehicle from dropdown

    if not selected_vehicle:
        status_label.config(text="No vehicle selected!", fg="red")
        return

    try:
        # Extract the vehicle name (everything before " (VIN...") and retrieve its VIN
        vehicle_name = selected_vehicle.split(" (")[0]  # Extract car name
        vin = vehicles[selected_vehicle]  # Get VIN from dictionary

        # Format MQTT topic dynamically using the vehicle name
        MQTT_TOPIC_CREATE = f"{vehicle_name.replace(' ', '')}"  # Remove spaces for MQTT topic format

        # Publish VIN only to the general command topic
        vin = int(vin)
        hex_vin = hex(vin)[3:]
        client.publish(MQTT_TOPIC_COMMANDS, hex_vin, qos=1)

        # # Log message to UI
        # status_label.config(text=f"publish topic set as {MQTT_TOPIC_CREATE}", fg="green")

        status_label.config(text=f"VIN {vin} activated", fg="green")

    except Exception as e:
        status_label.config(text=f"Error: {e}", fg="red")



def refresh_vehicle_dropdown():
    """ Refreshes the vehicle dropdown with the latest vehicles from the database """
    global vehicles
    vehicles = fetch_vehicles()  # Fetch updated vehicles
    vehicle_dropdown["values"] = list(vehicles.keys())  # Update dropdown options
    vehicle_dropdown.current(0)  # Reset selection


def refresh_file_dropdown():
    """ Refreshes the file dropdown with the latest files from the database """
    global files
    files = fetch_files()  # Fetch updated files
    file_dropdown["values"] = files  # Update dropdown options
    if files:
        file_dropdown.current(0)  # Reset selection to first file


def open_new_file_window():
    """ Opens a window to insert all required information for filesandcars table """
    file_window = tk.Toplevel(root)
    file_window.title("Insert New File Entry")
    file_window.geometry("400x400")

    # Labels & Entry Fields
    tk.Label(file_window, text="Car Name:", font=("Arial", 12)).pack(pady=5)
    carname_entry = tk.Entry(file_window, font=("Arial", 12))
    carname_entry.pack(pady=5, fill=tk.X, padx=20)

    tk.Label(file_window, text="Version:", font=("Arial", 12)).pack(pady=5)
    version_entry = tk.Entry(file_window, font=("Arial", 12))
    version_entry.pack(pady=5, fill=tk.X, padx=20)

    tk.Label(file_window, text="Select File:", font=("Arial", 12)).pack(pady=5)

    file_path_var = tk.StringVar()
    file_entry = tk.Entry(file_window, textvariable=file_path_var, font=("Arial", 12), state="readonly")
    file_entry.pack(pady=5, fill=tk.X, padx=20)

    def select_file():
        """ Opens a file dialog to select a file """
        file_path = filedialog.askopenfilename()
        if file_path:
            file_path_var.set(file_path)

    browse_button = tk.Button(file_window, text="Browse", command=select_file, bg="#2980B9", fg="white",
                              font=("Arial", 12))
    browse_button.pack(pady=5)

    def insert_file_entry():
        """ Inserts data into the filesandcars table """
        carname = carname_entry.get().strip()
        version = version_entry.get().strip()
        file_path = file_path_var.get().strip()

        if not (carname and version and file_path):
            tk.Label(file_window, text="All fields are required!", fg="red").pack()
            return

        try:
            # Read file as binary
            with open(file_path, "rb") as f:
                file_data = f.read()

            conn = connect_db()
            cursor = conn.cursor()
            cursor.execute("INSERT INTO filesandcars (carname, version, file) VALUES (%s, %s, %s)",
                           (carname, version, file_data))
            conn.commit()
            conn.close()

            # Refresh the file dropdown
            refresh_file_dropdown()

            tk.Label(file_window, text="File inserted successfully!", fg="green").pack()

        except Exception as e:
            tk.Label(file_window, text=f"Error: {e}", fg="red").pack()

    # Insert Button
    insert_btn = tk.Button(file_window, text="Insert File", command=insert_file_entry,
                           bg="#27AE60", fg="white", font=("Arial", 12))
    insert_btn.pack(pady=20)


def open_delete_vehicle_window():
    """ Opens a window to delete a vehicle entry from the carsandvin table """
    delete_window = tk.Toplevel(root)
    delete_window.title("Delete Vehicle Entry")
    delete_window.geometry("400x250")

    # Fetch latest vehicle list
    refresh_vehicle_dropdown()

    tk.Label(delete_window, text="Select Vehicle to Delete:", font=("Arial", 12)).pack(pady=5)

    vehicle_var = tk.StringVar()
    vehicle_dropdown_delete = ttk.Combobox(delete_window, textvariable=vehicle_var, values=list(vehicles.keys()),
                                           state="readonly", font=("Arial", 12))
    vehicle_dropdown_delete.pack(pady=5, fill=tk.X, padx=20)

    if vehicles:
        vehicle_dropdown_delete.current(0)  # Select first vehicle by default

    def delete_selected_vehicle():
        """ Deletes the selected vehicle from the database """
        selected_vehicle = vehicle_var.get()

        if not selected_vehicle:
            tk.Label(delete_window, text="No vehicle selected!", fg="red").pack()
            return

        # Extract VIN from selection
        vin = vehicles[selected_vehicle]

        try:
            conn = connect_db()
            cursor = conn.cursor()
            cursor.execute("DELETE FROM carsandvin WHERE vin = %s", (vin,))
            conn.commit()
            conn.close()

            # Refresh the vehicle dropdown
            refresh_vehicle_dropdown()

            tk.Label(delete_window, text="Vehicle deleted successfully!", fg="green").pack()
            vehicle_dropdown_delete["values"] = list(vehicles.keys())  # Update the dropdown after deletion

        except Exception as e:
            tk.Label(delete_window, text=f"Error: {e}", fg="red").pack()

    # Delete Button
    delete_btn = tk.Button(delete_window, text="Delete Vehicle", command=delete_selected_vehicle,
                           bg="#E74C3C", fg="white", font=("Arial", 12))
    delete_btn.pack(pady=20)


def open_delete_file_window():
        """ Opens a window to delete a file entry from the filesandcars table """
        delete_window = tk.Toplevel(root)
        delete_window.title("Delete File Entry")
        delete_window.geometry("400x250")

        # Fetch latest file list
        refresh_file_dropdown()

        tk.Label(delete_window, text="Select File to Delete:", font=("Arial", 12)).pack(pady=5)

        file_var = tk.StringVar()
        file_dropdown_delete = ttk.Combobox(delete_window, textvariable=file_var, values=files, state="readonly",
                                            font=("Arial", 12))
        file_dropdown_delete.pack(pady=5, fill=tk.X, padx=20)

        if files:
            file_dropdown_delete.current(0)  # Select first file by default

        def delete_selected_file():
            """ Deletes the selected file from the database """
            selected_file = file_var.get()

            if not selected_file:
                tk.Label(delete_window, text="No file selected!", fg="red").pack()
                return

            # Extract File ID from selection
            file_id = selected_file.split("File ID: ")[-1]

            try:
                conn = connect_db()
                cursor = conn.cursor()
                cursor.execute("DELETE FROM filesandcars WHERE id = %s", (file_id,))
                conn.commit()
                conn.close()

                # Refresh the file dropdown
                refresh_file_dropdown()

                tk.Label(delete_window, text="File deleted successfully!", fg="green").pack()
                file_dropdown_delete["values"] = files  # Update the dropdown after deletion

            except Exception as e:
                tk.Label(delete_window, text=f"Error: {e}", fg="red").pack()

        # Delete Button
        delete_btn = tk.Button(delete_window, text="Delete File", command=delete_selected_file,
                               bg="#E74C3C", fg="white", font=("Arial", 12))
        delete_btn.pack(pady=20)


def open_car_update_window():
    """ Opens a new window to insert or update vehicle details in carsandvin table """
    car_window = tk.Toplevel(root)
    car_window.title("New Vehicle")
    car_window.geometry("400x300")

    # Labels & Entry Fields
    tk.Label(car_window, text="VIN:", font=("Arial", 12)).pack(pady=5)
    vin_entry = tk.Entry(car_window, font=("Arial", 12))
    vin_entry.pack(pady=5, fill=tk.X, padx=20)

    tk.Label(car_window, text="Car Name:", font=("Arial", 12)).pack(pady=5)
    carname_entry = tk.Entry(car_window, font=("Arial", 12))
    carname_entry.pack(pady=5, fill=tk.X, padx=20)

    tk.Label(car_window, text="Details:", font=("Arial", 12)).pack(pady=5)
    details_entry = tk.Entry(car_window, font=("Arial", 12))
    details_entry.pack(pady=5, fill=tk.X, padx=20)

    def insert_update_car():
        """ Inserts or updates a car entry in the carsandvin table """
        vin = vin_entry.get().strip()
        carname = carname_entry.get().strip()
        details = details_entry.get().strip()

        if not (vin and carname and details):
            tk.Label(car_window, text="All fields are required!", fg="red").pack()
            return

        try:
            conn = connect_db()
            cursor = conn.cursor()

            # Check if VIN already exists
            cursor.execute("SELECT COUNT(*) FROM carsandvin WHERE vin = %s", (vin,))
            exists = cursor.fetchone()[0]

            if exists:
                cursor.execute("UPDATE carsandvin SET name=%s, details=%s WHERE vin=%s", (carname, details, vin))
                message = "Car details updated successfully!"
            else:
                cursor.execute("INSERT INTO carsandvin (vin, name, details) VALUES (%s, %s, %s)", (vin, carname, details))
                message = "New car added successfully!"

            conn.commit()
            conn.close()

            refresh_vehicle_dropdown()

            tk.Label(car_window, text=message, fg="green").pack()

        except Exception as e:
            tk.Label(car_window, text=f"Error: {e}", fg="red").pack()

    # Insert/Update Button
    insert_btn = tk.Button(car_window, text="Save Car", command=insert_update_car,
                           bg="#27AE60", fg="white", font=("Arial", 12))
    insert_btn.pack(pady=20)


def send_file():
    """ Fetches the selected file's contents from the database and sends it via MQTT to the selected vehicle's topic """
    selected_vehicle = vehicle_var.get()  # Get selected vehicle from dropdown
    selected_file = file_var.get()  # Get selected file from dropdown

    if not selected_vehicle:
        status_label.config(text="No vehicle selected!", fg="red")
        return
    if not selected_file:
        status_label.config(text="No file selected!", fg="red")
        return

    # Extract File ID from selection
    file_id = selected_file.split("File ID: ")[-1]

    try:
        conn = connect_db()
        cursor = conn.cursor()

        # Retrieve VIN and car name based on selected vehicle
        cursor.execute("SELECT name FROM carsandvin WHERE vin = %s", (vehicles[selected_vehicle],))
        car_result = cursor.fetchone()

        # Retrieve file content from filesandcars table
        cursor.execute("SELECT file FROM filesandcars WHERE id = %s", (file_id,))
        file_result = cursor.fetchone()

        conn.close()

        if car_result and file_result:
            car_name = car_result[0]  # Get car name
            file_content = file_result[0]  # Get file data

            # Format MQTT topic dynamically using the selected vehicle's name
            mqtt_topic = f"{car_name.replace(' ', '')}"  # Removing spaces for MQTT topic format

            # Try to decode as text, else send as binary
            try:
                decoded_content = file_content.decode("utf-8")  # Attempt text decoding
                display_text = f"File ID: {file_id}\n{decoded_content}"
                mqtt_payload = decoded_content  # Send as text
            except UnicodeDecodeError:
                display_text = f"File ID: {file_id}\n(Binary file - {len(file_content)} bytes)"
                mqtt_payload = file_content  # Send as binary

            # Display file content in UI
            file_display.insert(tk.END, display_text + "\n\n")
            file_display.see(tk.END)

            # Send file content over MQTT dynamically based on the selected vehicle
            client.publish(MQTT_TOPIC_CREATE, mqtt_payload, qos=1)
            status_label.config(text=f"File sent to MQTT topic: {mqtt_topic}", fg="green")

        else:
            status_label.config(text="Error: File or Vehicle Name not found!", fg="red")

    except Exception as e:
        status_label.config(text=f"Error: {e}", fg="red")


def send_command():
    selected_command = command_var.get().split(" - ")[0]  # Extract only the command number
    int_command = int(selected_command)
    hex_command = hex(int_command)[3:]
    status_label.config(text=f"Sent Command {selected_command}", fg="green")
    client.publish(MQTT_TOPIC_COMMANDS, hex_command)


# Create the GUI
root = tk.Tk()
root.title("MQTT Vehicle Interface")
root.geometry("1000x500")

# Sidebar (Larger size for left side UI)
sidebar = tk.Frame(root, width=600, bg="#2C3E50")
sidebar.pack(side=tk.LEFT, fill=tk.Y)

vehicle_frame = tk.Frame(sidebar, bg="#34495E", padx=10, pady=10)
vehicle_frame.pack(pady=10, fill=tk.X)

vehicle_label = tk.Label(vehicle_frame, text="Select Vehicle:", fg="white", bg="#34495E", font=("Arial", 14))
vehicle_label.pack(pady=5)

vehicle_var = tk.StringVar()
vehicle_dropdown = ttk.Combobox(vehicle_frame, textvariable=vehicle_var, values=list(vehicles.keys()), state="readonly", font=("Arial", 12))
vehicle_dropdown.pack(pady=5, fill=tk.X)
vehicle_dropdown.current(0)

activate_vin_button = tk.Button(vehicle_frame, text="Activate VIN", command=activate_vin, bg="#3498DB", fg="white", font=("Arial", 12))
activate_vin_button.pack(pady=10, fill=tk.X)

# File Selection Dropdown
file_frame = tk.Frame(sidebar, bg="#34495E", padx=10, pady=10)
file_frame.pack(pady=10, fill=tk.X)

file_label = tk.Label(file_frame, text="Select File:", fg="white", bg="#34495E", font=("Arial", 14))
file_label.pack(pady=5)

file_var = tk.StringVar()
file_dropdown = ttk.Combobox(file_frame, textvariable=file_var, values=files, state="readonly", font=("Arial", 12))
file_dropdown.pack(pady=5, fill=tk.X)
file_dropdown.current(0)

# "Send File" Button Below File Dropdown
send_file_button = tk.Button(file_frame, text="Send File", command=send_file, bg="#3498DB", fg="white", font=("Arial", 12))
send_file_button.pack(pady=10, fill=tk.X)


# Command Selection dropdown
command_frame = tk.Frame(sidebar, bg="#34495E", padx=10, pady=10)
command_frame.pack(pady=10, fill=tk.X)

command_label = tk.Label(command_frame, text="Select Command:", fg="white", bg="#34495E", font=("Arial", 14))
command_label.pack(pady=5)

command_var = tk.StringVar()
command_dropdown = ttk.Combobox(command_frame, textvariable=command_var, values=commands, state="readonly", font=("Arial", 12))
command_dropdown.pack(pady=5, fill=tk.X)
command_dropdown.current(0)

# Send command Button
send_command_button = tk.Button(command_frame, text="Send Command", command=send_command, bg="#3498DB", fg="white", font=("Arial", 12))
send_command_button.pack(pady=10, fill=tk.X)


# Main content
content_frame = tk.Frame(root)
content_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

status_label = tk.Label(content_frame, text="", font=("Arial", 12))
status_label.pack(pady=5)

message_display_label = tk.Label(content_frame, text="Received Messages:", font=("Arial", 12))
message_display_label.pack(pady=5)

message_display = tk.Text(content_frame, font=("Arial", 12), height=6, width=50)
message_display.pack(pady=5)


# File Content Display Box (Below "Received Messages")
file_display_label = tk.Label(content_frame, text="File Contents:", font=("Arial", 12))
file_display_label.pack(pady=5)

file_display = tk.Text(content_frame, font=("Arial", 12), height=6, width=50)
file_display.pack(pady=5)

# Add Button to Open Car Update Window
new_car_button = tk.Button(sidebar, text="New Vehicle", command=open_car_update_window,
                           bg="#F39C12", fg="white", font=("Arial", 12))
new_car_button.pack(side=tk.BOTTOM, pady=10, fill=tk.X)

new_file_button = tk.Button(sidebar, text="New File", command=open_new_file_window,
                            bg="#1ABC9C", fg="white", font=("Arial", 12))
new_file_button.pack(side=tk.BOTTOM, pady=5, fill=tk.X)

delete_vehicle_button = tk.Button(sidebar, text="Delete Vehicle", command=open_delete_vehicle_window,
                                  bg="#D35400", fg="white", font=("Arial", 12))
delete_vehicle_button.pack(side=tk.BOTTOM, pady=5, fill=tk.X)

# Add "Delete File" Button Below "New File"
delete_file_button = tk.Button(sidebar, text="Delete File", command=open_delete_file_window,
                               bg="#C0392B", fg="white", font=("Arial", 12))
delete_file_button.pack(side=tk.BOTTOM, pady=5, fill=tk.X)

threading.Thread(target=mqtt_subscribe, daemon=True).start()

root.mainloop()
