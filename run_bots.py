import subprocess
import time

BOT_COUNT = 5  # Cambia este número para más bots

for i in range(BOT_COUNT):
    subprocess.Popen(["python", "bot_client.py"])
    time.sleep(0.2)  # Pequeña pausa entre lanzamientos