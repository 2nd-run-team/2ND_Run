"""작성자 : 임진혁
보유 팩에 통일된 기능 아이콘이 없어 직접 그린 8개 UI 도형.
PNG는 Saved에만 생성하고 setup_p5.py가 Unreal AssetTools로 Texture2D를 임포트한다.
"""
from pathlib import Path
from PIL import Image,ImageDraw
OUT=Path(__file__).resolve().parents[2]/'Saved/Prototype01/P5Import'
OUT.mkdir(parents=True,exist_ok=True)
for name in ['Supply','Parts','Alloy','Core','S01','PingLocation','PingCargo','PingDanger']:
 im=Image.new('RGBA',(256,256));d=ImageDraw.Draw(im);c=(255,255,255,255);w=14
 if name=='Supply':
  d.rounded_rectangle((30,46,226,224),radius=22,outline=c,width=w);d.line((90,46,90,26,166,26,166,46),fill=c,width=w)
  d.rectangle((114,82,142,190),fill=c);d.rectangle((74,122,182,150),fill=c)
 elif name=='Parts':
  d.polygon([(48,22),(84,58),(72,88),(42,100),(8,68),(18,124),(54,144),(80,138),(184,242),(218,208),(114,104),(120,76),(96,36)],outline=c,width=w)
  d.ellipse((180,196,202,218),fill=c)
 elif name=='Alloy':
  for y in [40,105,170]:d.polygon([(26,y+36),(48,y),(204,y),(230,y+36)],outline=c,width=w)
 elif name=='Core':
  d.polygon([(84,18),(172,18),(238,84),(238,172),(172,238),(84,238),(18,172),(18,84)],outline=c,width=w)
  d.polygon([(128,62),(188,128),(128,194),(68,128)],outline=c,width=12);d.ellipse((112,112,144,144),fill=c)
 elif name=='S01':
  d.ellipse((58,10,198,246),outline=c,width=12);d.ellipse((10,64,246,192),outline=c,width=12);d.ellipse((94,94,162,162),fill=c)
 elif name=='PingLocation':
  d.ellipse((44,12,212,180),outline=c,width=w);d.polygon([(60,150),(128,246),(196,150)],outline=c,width=w);d.ellipse((101,69,155,123),fill=c)
 elif name=='PingCargo':
  d.polygon([(26,66),(128,16),(230,66),(230,190),(128,240),(26,190)],outline=c,width=w)
  d.line((26,66,128,120,230,66),fill=c,width=w);d.line((128,120,128,240),fill=c,width=w)
 else:
  d.polygon([(128,18),(240,230),(16,230)],outline=c,width=w);d.rounded_rectangle((116,82,140,164),radius=8,fill=c);d.ellipse((115,184,141,210),fill=c)
 im.save(OUT/('T_SP1'+name+'.png'))
print('P5 icons:',OUT)
