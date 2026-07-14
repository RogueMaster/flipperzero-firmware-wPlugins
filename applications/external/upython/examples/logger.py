import logging

logging.setLevel(logging.TRACE)

logging.trace("trace")
logging.debug("debug %d %s", 123, "message")
logging.info("info %d %s", 123, "message")
logging.warn("warn %d %s", 123, "message")
logging.error("error %d %s", 123, "message")

logging.log(logging.TRACE, "level: %d", logging.TRACE)
logging.log(logging.DEBUG, "level: %d", logging.DEBUG)
logging.log(logging.INFO, "level: %d", logging.INFO)
logging.log(logging.WARN, "level: %d", logging.WARN)
logging.log(logging.ERROR, "level: %d", logging.ERROR)
