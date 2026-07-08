"""手势识别结果 → STM32 USB CDC 发送模块"""
import time
import json as _json

NAMES = {0:"grabbing",1:"grip",2:"holy",3:"point",4:"call",5:"three3",
6:"timeout",7:"xsign",8:"hand_heart",9:"hand_heart2",10:"little_finger",
11:"middle_finger",12:"take_picture",13:"dislike",14:"fist",15:"four",
16:"like",17:"mute",18:"ok",19:"one",20:"palm",21:"peace",
22:"peace_inverted",23:"rock",24:"stop",25:"stop_inverted",26:"three",
27:"three2",28:"two_up",29:"two_up_inverted",30:"three_gun",31:"thumb_index",
32:"thumb_index2",33:"no_gesture"}

class USBSender:
    def __init__(self, port='/dev/stm32', baudrate=115200):
        self.count = 0
        self.last_time = 0
        self.ser = None
        try:
            import serial
            self.ser = serial.Serial(port, baudrate, timeout=0.1)
            print(f"[USB CDC] {port} 已连接")
        except Exception as e:
            print(f"[USB CDC] 失败: {e}")

    def send_detection(self, detection):
        """发送识别结果到 STM32 — JSON 行协议"""
        if detection.get('score', 0) < 0.5:
            return
        cid = detection.get('class_id', -1)
        name = NAMES.get(cid, f"ID{cid}")
        payload = {
            "c": cid,
            "n": name,
            "s": round(detection.get('score', 0), 4),
        }
        bbox = detection.get('bbox', None)
        if bbox and len(bbox) == 4:
            payload["x"] = int(bbox[0])
            payload["y"] = int(bbox[1])
            payload["w"] = int(bbox[2])
            payload["h"] = int(bbox[3])

        line = _json.dumps(payload, ensure_ascii=False) + '\n'
        now = time.time()
        if now - self.last_time < 0.1:
            return
        self.last_time = now
        self.count += 1
        print(f">>> [USB #{self.count:04d}] {name}(ID={cid}) score={payload['s']:.2f}")
        if self.ser:
            self.ser.write(line.encode('utf-8'))
            self.ser.flush()

    def close(self):
        if self.ser:
            self.ser.close()
        print(f"[USB CDC] 关闭(共{self.count}帧)")
