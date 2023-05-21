from tkinter import *
from tkinter import ttk
import struct
from serial.tools.list_ports import comports
from serial import Serial


def getLenFromEffect(effect):
    ln = effects[effect]["buffer"]
    for param in effects[effect]["params"]:
        ln += paramSizes[effects[effect]["params"][param]]
    return ln


paramSizes = {
    "pointer": 4,
    "int": 4,
    "float": 4
}

effects = dict()
effects["return"] = {
    "id": 255,
    "buffer": 0,
    "params": {}
}
effects["gain"] = {
    "id": 11,
    "buffer": 0,
    "params": {
        "gain":  "float"
    }
}
effects["noise suppr"] = {
    "id": 12,
    "buffer": paramSizes["pointer"],
    "params": {
        "threshold": "float"
    }
}
effects["distortion"] = {
    "id": 1,
    "buffer": 0,
    "params": {
        "gain": "float"
    }
}
effects["soft dist"] = {
    "id": 2,
    "buffer": 0,
    "params": {
        "gain": "float"
    }
}
effects["fuzz"] = {
    "id": 13,
    "buffer": 1,
    "params": {}
}
effects["vibrato"] = {
    "id": 3,
    "buffer": paramSizes["pointer"] + paramSizes["int"],
    "params": {
        "frequency": "float",
        "strength": "float"
    }
}
effects["chorus"] = {
    "id": 4,
    "buffer": paramSizes["pointer"] + paramSizes["int"],
    "params": {
        "frequency": "float",
        "strength": "float"
    }
}
effects["echo"] = {
    "id": 5,
    "buffer": paramSizes["pointer"] + paramSizes["int"],
    "params": {
        "delay": "float",
        "attenuation": "float"
    }
}
effects["low pass"] = {
    "id": 6,
    "buffer": paramSizes["pointer"],
    "params": {
        "cutoff freq": "float"
    }
}
effects["high pass"] = {
    "id": 7,
    "buffer": paramSizes["pointer"],
    "params": {
        "cutoff freq": "float"
    }
}
effects["tremolo"] = {
    "id": 8,
    "buffer": 0,
    "params": {
        "frequency": "float",
        "strength": "float"
    }
}
effects["rotary"] = {
    "id": 9,
    "buffer": paramSizes["pointer"] + paramSizes["int"],
    "params": {
        "frequency": "float",
        "vibrato strgth": "float",
        "tremolo strgth": "float"
    }
}
effects["reverb"] = {
    "id": 10,
    "buffer": 6 * getLenFromEffect("echo"),
    "params": {
        "dry-wet": "float",
        "reverb time": "float"
    }
}
effects["octave"] = {
    "id": 14,
    "buffer": paramSizes["pointer"],
    "params": {
        "dry-wet": "float"
    }
}
effects["mute"] = {
    "id": 0,
    "buffer": 0,
    "params": {}
}


class ScrollableFrame(ttk.Frame):
    def __init__(self, container, width=None, padding=0, **kwargs):
        super().__init__(container, **kwargs)
        canvas = Canvas(self, bd=0, highlightthickness=0, width=width)
        scrollbar = ttk.Scrollbar(self, orient="vertical", command=canvas.yview)
        self.scrollableArea = ttk.Frame(canvas, padding=padding)

        self.scrollableArea.bind(
            "<Configure>",
            lambda e: canvas.configure(
                scrollregion=canvas.bbox("all")
            )
        )

        canvas.create_window((0, 0), window=self.scrollableArea, anchor="nw")

        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")


class EffectDisplay(ttk.Frame):
    def __init__(self, container, channel, effect, row):
        super().__init__(container, padding=10, relief=GROOVE)
        self.container = container
        self.channel = channel
        self.effect = effect
        self.row = row
        self.params = {}

        ttk.Label(self, text="---------------------------------").pack()
        if not effect == "return":
            self.radio = ttk.Radiobutton(self, text=effect, variable=effectSelected, value=str(channel) + str(row))
            self.radio.pack()
            self.radio.invoke()

            if not effect == "start":
                for param in effects[effect]["params"]:
                    self.params[param] = StringVar()
                    ttk.Label(self, text=f"{param} ({effects[effect]['params'][param]})").pack()
                    ttk.Entry(self, textvariable=self.params[param]).pack()
        else:
            ttk.Label(self, text=effect).pack()

        ttk.Label(self, text="---------------------------------").pack()
        self.redraw()

    def redraw(self):
        self.grid(row=self.row, padx=65, pady=20)
        if not self.effect == "return":
            self.radio.configure(value=(str(self.channel) + str(self.row)))

    def die(self):
        for w in self.pack_slaves():
            w.destroy()
        self.destroy()

    def __str__(self):
        return self.effect

    def __repr__(self):
        return f"<{self.effect}>"


channel1 = []
channel2 = []


def addEffect(effect):
    chnl = int(effectSelected.get()[0])
    row = int(effectSelected.get()[1:])
    if chnl == 1:
        channel1.insert(row, EffectDisplay(channel1Frame, 1, effect, row + 1))
        if not row == len(channel1) - 1:
            for effctDisp in channel1[row + 1:]:
                effctDisp.row += 1
                effctDisp.redraw()
    else:
        channel2.insert(row, EffectDisplay(channel2Frame, 2, effect, row + 1))
        if not row == len(channel2) - 1:
            for effctDisp in channel2[row + 1:]:
                effctDisp.row += 1
                effctDisp.redraw()


def removeEffect():
    chnl = int(effectSelected.get()[0])
    row = int(effectSelected.get()[1:])
    if row == 1:
        return
    if chnl == 1:
        channel1[row - 1].die()
        channel1.pop(row - 1)
        channel1[row - 2].radio.invoke()
        if not row - 1 == len(channel1):
            for effctDisp in channel1[row - 1:]:
                effctDisp.row -= 1
                effctDisp.redraw()
    else:
        channel2[row - 1].die()
        channel2.pop(row - 1)
        channel2[row - 2].radio.invoke()
        if not row - 1 == len(channel2):
            for effctDisp in channel2[row - 1:]:
                effctDisp.row -= 1
                effctDisp.redraw()


def sendConfig(data):
    padding = bytearray((0xF0 for _ in range(256)))
    for i in range(len(data)):
        padding[i] = data[i]
    data = bytes(padding)
    for i in range(0, 256, 64):
        usbHandle.write(data[i:i+64])


def receiveConfig():
    sendConfig(b"\xFE")
    data = b""
    for i in range(0, 256, 64):
        data += usbHandle.read(64)
    return data


def applyConfig():
    data = b''
    for channel in (channel1, channel2):
        for effect in channel[1:]:
            data += bytes([effects[effect.effect]["id"]])
            if effects[effect.effect]["buffer"]:
                data += bytes(effects[effect.effect]["buffer"])
            try:
                for param in effect.params:
                    if effects[effect.effect]["params"][param] == "int":
                        data += struct.pack("<i", int(effect.params[param].get()))
                    if effects[effect.effect]["params"][param] == "float":
                        data += struct.pack("<f", float(effect.params[param].get()))
            except ValueError:
                showMessage("invalid parameter")
                return
    showMessage("config applied!")
    sendConfig(data)


def getEffectFromId(effectId):
    for effect in effects:
        if effects[effect]["id"] == effectId:
            return effect


def loadConfig(data=None):
    if data is None:
        data = receiveConfig()
    for effect in channel1[1:]:
        effect.die()
    del channel1[1:]
    for effect in channel2[1:]:
        effect.die()
    del channel2[1:]

    i = 0
    row = 2
    while True:
        effect = getEffectFromId(data[i])
        effectDisplay = EffectDisplay(channel1Frame, 1, effect, row)
        channel1.append(effectDisplay)
        row += 1
        i += 1
        if effect == "return":
            break
        if effects[effect]["buffer"]:
            i += effects[effect]["buffer"]
        for param in effects[effect]["params"]:
            if effects[effect]["params"][param] == "int":
                effectDisplay.params[param].set(str(struct.unpack("<i", data[i:i + paramSizes["int"]])[0]))
                i += paramSizes["int"]
            elif effects[effect]["params"][param] == "float":
                effectDisplay.params[param].set(str(round(struct.unpack("<f", data[i:i + paramSizes["float"]])[0], 3)))
                i += paramSizes["float"]
    row = 2
    while True:
        effect = getEffectFromId(data[i])
        effectDisplay = EffectDisplay(channel2Frame, 2, effect, row)
        channel2.append(effectDisplay)
        row += 1
        i += 1
        if effect == "return":
            break
        if effects[effect]["buffer"]:
            i += effects[effect]["buffer"]
        for param in effects[effect]["params"]:
            if effects[effect]["params"][param] == "int":
                effectDisplay.params[param].set(str(struct.unpack("<i", data[i:i + paramSizes["int"]])[0]))
                i += paramSizes["int"]
            elif effects[effect]["params"][param] == "float":
                effectDisplay.params[param].set(str(round(struct.unpack("<f", data[i:i + paramSizes["float"]])[0], 3)))
                i += paramSizes["float"]

    effectSelected.set("11")
    showMessage("config loaded!")


def showMessage(message):
    messageLabel.configure(text=message)


class FakeUsbHandle:
    def __init__(self):
        self.data = b'\n\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80?\x00\x00\x80?\xff\x00\xff'

    def read(self, n):
        return self.data

    def write(self, data):
        print(data)


root = Tk()
root.title("DSP gui")
root.geometry("800x600")
root.resizable(False, False)

effectSelected = StringVar()

style = ttk.Style(root)
style.configure("lightgray.TFrame", background="#DDDDDD")
style.configure("lightgray.TLabel", background="#DDDDDD")

masterFrame = ttk.Frame(root)
masterFrame.pack(fill=BOTH, expand=True)

bottomFrame = ttk.Frame(masterFrame, style="lightgray.TFrame")
bottomFrame.pack(side=BOTTOM, fill=X)

ttk.Button(bottomFrame, text="Apply", command=applyConfig).pack(side=RIGHT, padx=10, pady=10)
ttk.Button(bottomFrame, text="Load", command=loadConfig).pack(side=RIGHT, padx=0, pady=10)
ttk.Button(bottomFrame, text="Delete", command=removeEffect).pack(side=LEFT, padx=10, pady=10)
messageLabel = ttk.Label(bottomFrame, text="", style="lightgray.TLabel")
messageLabel.pack(side=RIGHT, padx=10, pady=10)

upperFrame = ttk.Frame(masterFrame)
upperFrame.pack(side=BOTTOM, fill=BOTH, expand=True)

leftFrame = ScrollableFrame(upperFrame, width=125, padding=10)
leftFrame.pack(side=LEFT, fill=Y)

for effect in effects:
    if not effect == "return":
        button = ttk.Button(leftFrame.scrollableArea, text=effect, command=lambda e=effect: addEffect(e), padding=5)
        button.pack(side=TOP, padx=10, pady=10)

rightFrame = ScrollableFrame(upperFrame)
rightFrame.pack(side=LEFT, fill=BOTH, expand=True)

channel1Frame = ttk.Frame(rightFrame.scrollableArea)
channel1Frame.pack(side=LEFT, fill=Y)
channel2Frame = ttk.Frame(rightFrame.scrollableArea)
channel2Frame.pack(side=LEFT, fill=Y)

ttk.Label(channel1Frame, text="channel 1").grid(row=0)
ttk.Label(channel2Frame, text="channel 2").grid(row=0)
channel1.append(EffectDisplay(channel1Frame, 1, "start", 1))
channel2.append(EffectDisplay(channel2Frame, 2, "start", 1))
loadConfig(data=b'\x00\xff\x00\xff')

try:
    usbPort = comports()[0].name
    usbHandle = Serial(port=usbPort, baudrate=115200)
    if not usbHandle.is_open:
        usbHandle.open()
    showMessage("usb connected")
except IndexError:
    showMessage("loading with fake usb")
    usbHandle = FakeUsbHandle()

root.mainloop()
