import pyaudio
import numpy as np

class AudioProcessor:
    def __init__(self):
        self.p = pyaudio.PyAudio()
        self.stream = None
        self.volume = 0

    def start_listening(self):
        self.stream = self.p.open(format=pyaudio.paInt16,
                                  channels=1,
                                  rate=44100,
                                  input=True,
                                  frames_per_buffer=1024,
                                  stream_callback=self.audio_callback)
        self.stream.start_stream()

    def stop_listening(self):
        if self.stream:
            self.stream.stop_stream()
            self.stream.close()
            self.stream = None

    def audio_callback(self, in_data, frame_count, time_info, status):
        data = np.frombuffer(in_data, dtype=np.int16)
        self.volume = np.sqrt(np.mean(data**2))  # RMS音量计算
        return (in_data, pyaudio.paContinue)

    def get_current_volume(self):
        return self.volume

    def __del__(self):
        self.p.terminate()