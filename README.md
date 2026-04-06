# Panel-Link USB Graphics Driver

A USB graphics driver based on **IddCx (Indirect Display Driver Class Extension)** and **UMDF**, designed for BeadaPanel USB display devices.  
This project provides a complete implementation of a USB‑to‑virtual‑display pipeline, including device communication, mode negotiation, and frame transmission.

---

## ✨ Features

- USB video transmission using **RGB16** pixel format  
- Virtual display output via **IddCx**  
- Multi‑resolution support  
- Automatic preferred‑mode selection based on panel model 
- Panel information query (StatusLink protocol)  
- Panel brightness control  
- Fully open‑source UMDF driver implementation

---

## 📌 Project Status

This repository contains:

- Full driver source code  
- INF installation file  
- UMDF driver DLL  
- Prebuilt binaries under `x64/Release/`  
- GPL V3 license  

Developers can rebuild the driver using Visual Studio 2022 + WDK.

---

## 📄 Documentation

To keep documentation clean and easy to update, detailed technical documents are hosted externally:

- **USB Protocol Specification**  
  https://nxelec.com/documents/bp/Panel-Link_USB_Media_Stream_Transport_Protocol_Rev10.pdf
  https://nxelec.com/documents/bp/Status-Link_USB_Panel_Control_Protocol_Rev13.pdf

- **Panel User Guide**  
  https://nxelec.com/documents/bp/BeadaPanel_User_Quick_Start_Guide_v1_3.pdf

> Build instructions and debugging guides will be added later.

---

## 🖥 Supported Hardware

- BeadaPanel USB display series  
- Firmware version requirement: **≥ 700**  
- USB Bulk IN/OUT communication  
- StatusLink / PanelLink protocol support  

---

## 📦 Driver Files

The driver package includes:

- `panel-link_usb_graphics.dll` — UMDF driver  
- `panel-link_usb_graphics.inf` — installation file  
- `panel-link_usb_graphics.cat` — catalog file (if signed)  

---

## 🔧 Build (Short Version)

> Full build guide will be added later.

- Visual Studio 2022  
- Windows Driver Kit (WDK)  
- Open the solution and build `x64 / Release`  
- Output binaries will appear in the Release folder  

---

## 📜 License

This project is licensed under the **GPL V3 License**.  
See the `LICENSE` file for details.

---

## 🤝 Contributing

Issues and pull requests are welcome.  
A detailed contributing guide will be added in the future.

