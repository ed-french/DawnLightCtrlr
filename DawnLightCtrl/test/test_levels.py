import requests
import time

url="http://192.168.1.164/override?seconds=120&level="


while True:
    for i in range(36):
        r=requests.get(url+str(i))
        print(i)
        time.sleep(2)

    time.sleep(3)