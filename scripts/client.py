import httplib, urllib

f = open("img.jpg", "rb")
data = f.read()
f.close()

headers = {"content-type": "image/jpeg", "content-length": str(len(data))}
conn = httplib.HTTPConnection("localhost", 8080)
conn.request("POST", "api?m=sobelfilter", data, headers)
#conn.request("GET", "index.html")
response = conn.getresponse()
data = response.read()
conn.close()

f = open("sobel_img.jpg", "wb")
f.write(data)
f.close()

