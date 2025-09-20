import multiprocessing as mp
import time

class DSUServer:
    def __init__(self):
        def serve(child_conn):
            for i in range(5):
                [acc, gyro] = child_conn.recv()
                ts = time.time_ns()
                print(acc[0], acc[1], acc[2])
                print(gyro[0], gyro[1], gyro[2])
                print("ts", ts)
                
            child_conn.close()
    
        parent_conn, child_conn = mp.Pipe()
        self.process = mp.Process(target=serve, args=(child_conn,))
        self.process.start()
        
        self.parent_conn = parent_conn
    
    def update(self, acc, gyro):
        self.parent_conn.send([
            [acc[0], acc[1], acc[2]],
            [gyro[0], gyro[1], gyro[2]]
        ])
        
    def close(self):
        self.process.join()
        self.process.close()


def main():
    from math import sin, cos
    
    server = DSUServer()
    
    t = 0.0
    scale = 45.0
    g = 9.81

    try:
        while 1:
            server.update(
                [sin(t) * scale, cos(t) * scale, sin(t + 0.5) * scale],
                [sin(t) * g, cos(t) * g, 0.0]
            )
            
            t += 1.0 / 1024.0;
    except BrokenPipeError:
        # shutdown
        pass
    finally:
        server.close()

if __name__ == "__main__":
    main()
