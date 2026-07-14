Examples
========

This page contains a few examples.
See more on `GitHub <https://github.com/ofabel/mp-flipper/tree/master/examples>`_.

Speaker
-------

.. literalinclude:: ../../examples/flipperzero_speaker_test.py
   :language: python

For details, see the :ref:`reference-speaker` section on the :doc:`reference` page.

Input
-----

.. literalinclude:: ../../examples/flipperzero_draw_on_input_test.py
   :language: python

For details, see the :ref:`reference-input` section on the :doc:`reference` page.

Interrupts
----------

.. literalinclude:: ../../examples/flipperzero_gpio_interrupt_test.py
   :language: python

This example drives an external LED upon interrupts: A rising edge on ``C0`` sets the pin ``A7`` to high, a rising edge on ``C1`` sets the pin ``A7`` to low.
The following schematic circuit diagram shows the hardware setup for this example:

.. figure:: ./assets/gpio_interrupt_circuit.svg
   :width: 90%

   Hardware setup for the GPIO interrupt example.

For details, see the :ref:`reference-gpio` section on the :doc:`reference` page.

ADC
---

.. literalinclude:: ../../examples/flipperzero_adc_test.py
   :language: python

This example uses a voltage divider with the 3.3 V source from pin 9. The switch ``S1`` changes the input voltage on ``C1`` between 0 and about 0.8 V.

.. figure:: ./assets/adc_circuit.svg
   :width: 90%

   Hardware setup for the ADC example.

For details, see the :ref:`reference-adc` section on the :doc:`reference` page.

PWM
---

.. literalinclude:: ../../examples/flipperzero_pwm_test.py
   :language: python

This example drives an LED connected to pin ``A7`` and ``GND`` using a PWM signal with two different frequency and duty cycle settings.

.. figure:: ./assets/pwm_circuit.svg
   :width: 90%

   Hardware setup for the PWM example.

For details, see the :ref:`reference-pwm` section on the :doc:`reference` page.

Infrared
--------

.. literalinclude:: ../../examples/flipperzero_infrared_test.py
   :language: python

For details, see the :ref:`reference-infrared` section on the :doc:`reference` page.
