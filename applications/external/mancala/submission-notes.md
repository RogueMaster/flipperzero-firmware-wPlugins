## Submission to the Flipper Zero Application Catalog (FAC)

[flipper-application-catalog -> Contributing.md](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md) lists the guidelines for the submission to the FAC.

The challenging part (for me) always is this [validation of the manifest.yml](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Manifest.md#validating-manifest) 
using a non-Linux operating system.
```
python3 -m venv venv
source venv/bin/activate
pip install -r tools/requirements.txt
```
The above mentioned [requirements.txt](https://github.com/flipperdevices/flipper-application-catalog/blob/main/tools/requirements.txt) are simply:
```text
dataclass-wizard==0.26.0
Pillow==10.4.0
PyYAML==6.0.2
Markdown==3.7
ufbt>=0.2.5
requests==2.32.3
```

In **Powershell** one may download the `bundle.py` with
```
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/flipperdevices/flipper-application-catalog/refs/heads/main/tools/bundle.py" -OutFile "bundle.py"
```

Cloning the current FAC can be done (without password!) via
```
git clone https://github.com/flipperdevices/flipper-application-catalog.git
```
