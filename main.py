import time

from arduino.app_utils import *
from arduino.app_bricks.streamlit_ui import st
st.title("LED CONTROL")
if st.button("TURN LED ON"):
        Bridge.call("led_on")
        st.success("LED is ON")
if st.button("TURN LED OFF"):
        Bridge.call("led_off")
        st.success("LED is OFFF")


# See: https://docs.arduino.cc/software/app-lab/tutorials/getting-started/#app-run
App.run()
