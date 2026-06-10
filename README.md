# Flipper application for RadSens module

RadSens Application is not official application allows you to get level of current radiation measured by RadSens dosimeter module based on a Geiger tube.

The Application stores amount of particles between launches. RadSens counts particles when powered, so you can use your Flipper device and not miss any particles in background.

<div align="center">
<img src="images/meas_mode.jpg" width="275">
<img src="images/plot_mode.jpg" width="275">
</div>

## App Controls
- OK button turns on and off vibration mode when every new particle generates vibration impulse.
- Up and Down buttons change RadSens sensitivity in steps of `5 imp/uR`.
- Right button switches to Plot Mode.
- Left button gets back to Measurement Mode.

Current sensitivity is shown in Measurement Mode as `S:<value>`. The application writes the new
coefficient back to the RadSens module, so it is used for subsequent measurements.

## About RadSens

[RadSens official repository](https://github.com/climateguard/RadSens)

**You can buy RadSens at:**

-  [Tindie](https://www.tindie.com/stores/climateguard/)  
-  [Aliexpress](https://aliexpress.ru/store/all-wholesale-products/910985005.html)  
-  [Alibaba](https://mashintertorg.trustpass.alibaba.com/productgrouplist-903279422/Electronics.html?spm=a2700.shop_cp.88.14)

## RadSens enclosure

Find .stl files for 3d printing on [Thingiverse](https://www.thingiverse.com/thing:5841046)

## License

**Flipper_RadSens** is available under the MIT license. See the [LICENSE](https://github.com/sionyx/flipper_radsens/blob/main/LICENSE) file for more info.
