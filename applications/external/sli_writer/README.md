# SLI Writer — Magic ISO15693 UID Writer

I made a simple Flipper app to write **magic ISO15693 tags with changeable UID** using `.nfc` files.

---

## 📱 Update (26/03)

I also developed an **Android version** of the app, available as an `.apk`.

I can share the source code if anyone needs it.

---

## 🏷️ Supported Tags

### SLIX2 / ISO15693 (Magic UID)
You can use:
https://aliexpress.com/item/1005006949791537.html

⚠️ One user reported that **round tags are not working**,  
so it’s recommended to **stick with rectangular ones**.

---

### SLIX-L Tags
Use:
http://rfidfriend.com

---

## 🧠 Background

I originally tried to *vibecode* this app about 6 months ago but got stuck.  
It somehow ended up in Roguemaster firmware, although I’m not sure if it was ever fixed there.

Recently, I discovered the **"ISO 15693-3 NFC Writer"** app in Unleashed,  
which contained everything needed to fix my implementation.

The app is now **fully working**.

---

## ✍️ Write Sequence

### 1. Write data blocks
- Command: `WRITE_SINGLE_BLOCK`
- Mode: **non-addressed**
- Flags: `0x02`

---

### 2. Write UID (Gen2 vendor commands)

```
02 E0 09 40 <uid_high>   → sets bytes 0–3
02 E0 09 41 <uid_low>    → sets bytes 4–7
```

Equivalent to:
```
proxmark hf 15 csetuid -u <uid> --v2
```

---

### ⚠️ Important

The Gen2 layout command (`0x47`) is **intentionally NOT sent**.

- Not required for most readers
- **Will brick SLIX-L magic cards**

---

## 🛠️ Debugging

If writing fails:

1. Connect your Flipper via USB  
2. Open a serial console (Putty, screen, etc.)  
3. Set baud rate to:
```
230400
```

---

### Enable debug logs:

```
> log debug
```

Then look for:
```
[SLI_Writer]
```

---

### Key debug messages

```
iso_send_raw: err=0 rxbytes=2 resp=[XX YY]
```
- Command received
- Card returned an error
- `YY` = ISO15693 error code

```
iso_send_raw: err=6 rxbytes=0
```
- No response
- Likely timeout or CRC issue

```
Write block N failed
```
- Block write failed after retries

