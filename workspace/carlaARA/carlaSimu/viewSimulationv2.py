import carla
import numpy as np
import cv2
import random
import time
import math
import queue
import socket
import struct
import threading

# Server IP Address and Port
CARLA_SERVER_IP = "100.112.183.51"
# CARLA_SERVER_IP = "172.28.182.31"
PORT = 2000
# CARLA Map selection (e.g. 'Town03_Opt', 'Town01_Opt', 'Town05_Opt', 'Town10HD_Opt')
# Set to None to use whichever map is currently loaded
CARLA_MAP = "Town03_Opt"

actor_list = []

def find_4way_junction_and_spawn_point(world):
    map_obj = world.get_map()
    spawn_points = map_obj.get_spawn_points()
    
    # 1. Get all traffic lights on the map and group them by junction
    all_lights = world.get_actors().filter('traffic.traffic_light')
    groups = {}
    for light in all_lights:
        group_lights = light.get_group_traffic_lights()
        group_ids = sorted([l.id for l in group_lights])
        if group_ids:
            group_key = group_ids[0]
            groups[group_key] = group_lights

    # Find junction groups with exactly 4 traffic lights
    four_way_groups = [g for g in groups.values() if len(g) == 4]
    if not four_way_groups:
        # If no 4-way group, use any group as fallback
        four_way_groups = list(groups.values())
        
    if not four_way_groups:
        print("No traffic light junction groups found on the map.")
        return random.choice(spawn_points), None

    print(f"Found {len(four_way_groups)} traffic light junction groups.")
    # Select the first junction group as target
    target_lights = four_way_groups[0]
    
    # Calculate the center coordinates of the junction (average of light pole coordinates)
    xs = [l.get_location().x for l in target_lights]
    ys = [l.get_location().y for l in target_lights]
    zs = [l.get_location().z for l in target_lights]
    center = carla.Location(sum(xs)/len(xs), sum(ys)/len(ys), sum(zs)/len(zs))
    print(f"Junction center coordinates: x={center.x:.2f}, y={center.y:.2f}, z={center.z:.2f}")

    # 2. Find a spawn point on a straight line facing directly towards the junction
    candidate_sp = []
    for sp in spawn_points:
        dist = sp.location.distance(center)
        # Select a spawn point between 25 and 90 meters from the junction to ensure a sufficiently long straight path
        if 25.0 < dist < 90.0:
            # Vector direction from spawn point to junction
            dx = center.x - sp.location.x
            dy = center.y - sp.location.y
            len_xy = math.sqrt(dx**2 + dy**2)
            if len_xy == 0:
                continue
            vx = dx / len_xy
            vy = dy / len_xy
            
            # Vehicle heading vector (based on the spawn point's yaw)
            yaw_rad = math.radians(sp.rotation.yaw)
            fx = math.cos(yaw_rad)
            fy = math.sin(yaw_rad)
            
            # Dot product to check if the vehicle heading points straight into the junction
            dot = vx * fx + vy * fy
            if dot > 0.95:  # Deviation no more than 18 degrees from the straight line to the junction
                candidate_sp.append((sp, dist))
                
    if candidate_sp:
        # Select the furthest spawn point for the longest straight path towards the junction
        candidate_sp.sort(key=lambda x: x[1], reverse=True)
        chosen_sp = candidate_sp[0][0]
        print(f"Found ideal spawn point {candidate_sp[0][1]:.2f} meters from the junction.")
        return chosen_sp, center
    else:
        print("No straight-facing spawn point found, using a random point near the junction.")
        closest_sp = min(spawn_points, key=lambda sp: sp.location.distance(center))
        return closest_sp, center

try:
    print(f"Connecting to CARLA Server at {CARLA_SERVER_IP}:{PORT}...")
    client = carla.Client(CARLA_SERVER_IP, PORT)
    client.set_timeout(120.0)
    
    if CARLA_MAP:
        print(f"Loading map '{CARLA_MAP}' on Server (this might take 10-20 seconds)...")
        world = client.load_world(CARLA_MAP)
    else:
        world = client.get_world()
        
    print(f"Active Map: {world.get_map().name}")
    blueprint_library = world.get_blueprint_library()
    
    # 1. Find 4-way junction and a spawn point on a straight line heading into the junction
    spawn_point, junction_center = find_4way_junction_and_spawn_point(world)
    
    # 2. Spawn Tesla Model 3
    vehicle_bp = blueprint_library.filter('model3')[0]
    vehicle = world.spawn_actor(vehicle_bp, spawn_point)
    actor_list.append(vehicle)
    print(f"Spawned vehicle at: {spawn_point.location}")
    
    # Wait 1 second for the vehicle to initialize completely
    time.sleep(1.0)
    
    # Enable physics simulation for the vehicle (steer straight manually using Python code)
    print("Enabling physics simulation... Steering locked straight (steer=0.0).")
    vehicle.set_simulate_physics(True)
    vehicle.set_autopilot(False)
    
    # 3. Spawn rear camera (Chase Camera - 3rd Person View)
    print("Spawning 3rd-person Chase Camera...")
    camera_bp = blueprint_library.find('sensor.camera.rgb')
    camera_bp.set_attribute('image_size_x', '800')
    camera_bp.set_attribute('image_size_y', '600')
    camera_bp.set_attribute('fov', '90')
    
    # Place camera higher and behind the vehicle to clearly see the road and traffic lights
    camera_transform = carla.Transform(carla.Location(x=-5.5, z=2.8), carla.Rotation(pitch=-15))
    camera_chase = world.spawn_actor(camera_bp, camera_transform, attach_to=vehicle)
    actor_list.append(camera_chase)
    print("Successfully spawned 3rd-person Chase Camera!")
    
    # 3.2 Spawn front camera (Front Camera - 1st Person View)
    print("Spawning 1st-person Front Camera (3x Zoom)...")
    camera_front_bp = blueprint_library.find('sensor.camera.rgb')
    camera_front_bp.set_attribute('image_size_x', '800')
    camera_front_bp.set_attribute('image_size_y', '600')
    camera_front_bp.set_attribute('fov', '30') # 3x Zoom (FOV = 30 degrees instead of default 90 degrees)
    
    # Place camera at the vehicle front facing straight ahead
    camera_front_transform = carla.Transform(carla.Location(x=1.6, z=1.2), carla.Rotation(pitch=0))
    camera_front = world.spawn_actor(camera_front_bp, camera_front_transform, attach_to=vehicle)
    actor_list.append(camera_front)
    print("Successfully spawned 1st-person Front Camera!")
    
    # 4. Store images from data streams to display both windows simultaneously
    class ImageState:
        def __init__(self):
            self.chase_img = None
            self.front_img = None
            
    img_state = ImageState()
    
    def process_img_chase(image):
        i = np.array(image.raw_data)
        i2 = i.reshape((600, 800, 4))
        # Extract BGR channels and convert to contiguous array to prevent OpenCV drawing crash
        img_state.chase_img = np.ascontiguousarray(i2[:, :, :3])
        
    def process_img_front(image):
        i = np.array(image.raw_data)
        i2 = i.reshape((600, 800, 4))
        # Extract BGR channels and convert to contiguous array
        img_state.front_img = np.ascontiguousarray(i2[:, :, :3])

    print("Connecting image stream listeners...")
    # Stream 1: Receive 3rd-person view images
    camera_chase.listen(lambda image: process_img_chase(image))
    # Stream 2: Receive 1st-person view images
    camera_front.listen(lambda image: process_img_front(image))

    # TCP Helper to communicate with ProviderSensor in a background thread
    TCP_IP = "172.31.36.143"
    TCP_PORT = 5000

    class ProviderSensorTCPWorker:
        def __init__(self, ip, port):
            self.ip = ip
            self.port = port
            self.lock = threading.Lock()
            self.running = True
            self.new_frame = False
            
            # Inputs
            self.frame = None
            self.lat = 0.0
            self.lon = 0.0
            
            # Outputs
            self.dist = 999.0
            self.flag_traffic_light = "unknown"
            
        def submit_request(self, frame, lat, lon):
            with self.lock:
                self.frame = frame.copy() if frame is not None else None
                self.lat = lat
                self.lon = lon
                self.new_frame = True
                
        def get_latest_results(self):
            with self.lock:
                return self.dist, self.flag_traffic_light
                
        def run_loop(self):
            # Establish initial connection
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            connected = False
            print(f"Connecting to ProviderSensor TCP server at {self.ip}:{self.port}...")
            while self.running and not connected:
                try:
                    sock.connect((self.ip, self.port))
                    connected = True
                    print("Connected to ProviderSensor TCP server successfully!")
                except Exception as e:
                    time.sleep(1.0)
                    
            while self.running:
                frame_to_send = None
                lat_to_send = 0.0
                lon_to_send = 0.0
                
                with self.lock:
                    if self.new_frame and self.frame is not None:
                        frame_to_send = self.frame
                        lat_to_send = self.lat
                        lon_to_send = self.lon
                        self.new_frame = False
                         
                if frame_to_send is not None:
                    try:
                        # Encode image to JPEG
                        _, jpeg_data = cv2.imencode('.jpg', frame_to_send, [int(cv2.IMWRITE_JPEG_QUALITY), 95])
                        jpeg_bytes = jpeg_data.tobytes()
                        
                        # Pack header: FPV1 (4s), lat (double), lon (double), jpeg_len (uint32)
                        header = struct.pack('!4sddI', b'FPV1', lat_to_send, lon_to_send, len(jpeg_bytes))
                        
                        sock.sendall(header + jpeg_bytes)
                        
                        # Read response (24 bytes)
                        response = b''
                        while len(response) < 24 and self.running:
                            chunk = sock.recv(24 - len(response))
                            if not chunk:
                                raise ConnectionError("Socket closed by peer")
                            response += chunk
                            
                        if len(response) == 24:
                            dist, flag_bytes = struct.unpack('!d16s', response)
                            flag = flag_bytes.decode('utf-8').strip('\x00').strip()
                            with self.lock:
                                self.dist = dist
                                self.flag_traffic_light = flag
                    except Exception as e:
                        print(f"TCP Worker thread socket error: {e}. Reconnecting...")
                        sock.close()
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        while self.running:
                            try:
                                sock.connect((self.ip, self.port))
                                print("Reconnected to ProviderSensor TCP server!")
                                break
                            except Exception:
                                time.sleep(1.0)
                else:
                    time.sleep(0.01) # Avoid tight CPU loop
                    
            try:
                sock.close()
            except:
                pass

    # Start TCP background worker thread
    worker = ProviderSensorTCPWorker(TCP_IP, TCP_PORT)
    worker_thread = threading.Thread(target=worker.run_loop, daemon=True)
    worker_thread.start()

    print("Displaying simulation windows... Press Ctrl+C or 'q' on the window to exit.")
    
    # 5. GUI display loop on the main thread
    is_teleporting = False
    dist = 999.0
    flag_traffic_light = "unknown"
    target_speed = 40.0
    current_speed = 0.0
    
    while True:
        # Get Chase image (3rd Person View) from shared state
        img_chase = img_state.chase_img
        if img_chase is not None:
            # Create copy of the image to draw info on screen avoiding stream read/write conflicts
            img_chase_disp = img_chase.copy()
            
            # Draw TCP status circle and info on top-left of screen
            if flag_traffic_light == "red":
                color = (0, 0, 255) # BGR Red (Pure Red)
            elif flag_traffic_light == "yellow":
                color = (0, 128, 255) # BGR Orange/Amber (Highly distinct from Red)
            elif flag_traffic_light == "green":
                color = (0, 255, 0) # BGR Green
            else:
                color = (200, 200, 200) # Light Gray for unknown
            
            cv2.circle(img_chase_disp, (50, 50), 18, color, -1)
            cv2.putText(img_chase_disp, f"Traffic Light (TCP): {flag_traffic_light.upper()}", (80, 48), cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
            cv2.putText(img_chase_disp, f"Distance to Light: {dist:.1f}m", (80, 78), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            cv2.putText(img_chase_disp, f"Target Speed: {target_speed:.1f} km/h", (80, 108), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            cv2.putText(img_chase_disp, f"Current Speed: {current_speed:.1f} km/h", (80, 138), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
                
            cv2.imshow("CARLA Third-Person View (Chase)", img_chase_disp)
            
        # Get Front image (1st Person View, 3x zoom) from shared state
        img_front = img_state.front_img
        if img_front is not None:
            cv2.imshow("CARLA First-Person View (Zoom 3x)", img_front)
            
        # 5.1 Control the vehicle to drive straight using code (no Autopilot)
        if not is_teleporting:
            if img_front is not None:
                # Get vehicle geolocation
                veh_loc = vehicle.get_location()
                geo_loc = world.get_map().transform_to_geolocation(veh_loc)
                lat = geo_loc.latitude
                lon = geo_loc.longitude
                
                # Submit request to background worker thread (non-blocking)
                worker.submit_request(img_front, lat, lon)
                
                # Get the latest results computed asynchronously
                dist, flag_traffic_light = worker.get_latest_results()
                
                # Calculate target speed
                max_speed = 40.0  # km/h
                target_speed = max_speed
                
                # If traffic light is red or yellow, and we are within 200m, slow down
                if flag_traffic_light in ["red", "yellow"]:
                    if dist < 200.0:
                        # Linearly decrease target speed from max_speed at 200m to 0 at 10m
                        target_speed = max(0.0, ((dist - 10.0) / 190.0) * max_speed)
                        if dist <= 10.0:
                            target_speed = 0.0
                
                # Simple proportional speed controller
                vel = vehicle.get_velocity()
                current_speed = 3.6 * math.sqrt(vel.x**2 + vel.y**2 + vel.z**2)
                
                if target_speed == 0.0:
                    control = carla.VehicleControl(throttle=0.0, steer=0.0, brake=1.0)
                else:
                    speed_error = target_speed - current_speed
                    if speed_error > 0:
                        throttle = min(0.6, 0.2 + 0.05 * speed_error)
                        control = carla.VehicleControl(throttle=throttle, steer=0.0, brake=0.0)
                    else:
                        brake = min(1.0, 0.1 * abs(speed_error))
                        control = carla.VehicleControl(throttle=0.0, steer=0.0, brake=brake)
                
                vehicle.apply_control(control)
            else:
                # Initial frames where front camera image is not ready yet
                control = carla.VehicleControl(throttle=0.1, steer=0.0, brake=0.0)
                vehicle.apply_control(control)
            
        # 6. Automatically teleport vehicle to start line if it has passed the junction (Teleport Loop)
        if junction_center is not None:
            veh_loc = vehicle.get_location()
            
            if is_teleporting:
                # If teleporting, check if the vehicle is close to the spawn point
                if veh_loc.distance(spawn_point.location) < 5.0:
                    is_teleporting = False
                    print("Vehicle returned to spawn point successfully!")
            else:
                # Only check junction crossing if not currently teleporting
                dist_to_junction = veh_loc.distance(junction_center)
                
                # Vector direction from junction to vehicle
                dx_past = veh_loc.x - junction_center.x
                dy_past = veh_loc.y - junction_center.y
                
                # Current movement direction of the vehicle
                yaw_rad = math.radians(vehicle.get_transform().rotation.yaw)
                fx = math.cos(yaw_rad)
                fy = math.sin(yaw_rad)
                
                # Dot product to determine if vehicle has passed the junction and is moving away
                dot_past = dx_past * fx + dy_past * fy
                
                if dist_to_junction > 15.0 and dot_past > 0:
                    print("Vehicle passed junction! Teleporting back to start line...")
                    is_teleporting = True
                    
                    # Reset vehicle control inputs (steer to 0, brake)
                    control = carla.VehicleControl(steer=0.0, throttle=0.0, brake=1.0)
                    vehicle.apply_control(control)
                    
                    # Teleport vehicle to spawn point
                    vehicle.set_transform(spawn_point)
                    
                    # Reset physical velocity (linear and angular) completely to 0
                    vehicle.set_target_velocity(carla.Vector3D(0, 0, 0))
                    vehicle.set_target_angular_velocity(carla.Vector3D(0, 0, 0))
                    
                    # Wait 0.15 seconds for Server physics engine to synchronize new position
                    time.sleep(0.15)
            
        # Check keyboard shortcut event (wait 10ms to reduce CPU load)
        if cv2.waitKey(10) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\nShutting down simulation client...")
except Exception as e:
    print("Error occurred:", e)
finally:
    print("Releasing resources on Server...")
    try:
        worker.running = False
    except:
        pass
    for actor in actor_list:
        try:
            actor.destroy()
        except:
            pass
    cv2.destroyAllWindows()
    print("Cleanup complete!")
