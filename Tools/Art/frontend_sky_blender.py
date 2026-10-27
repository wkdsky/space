"""Paint an original clear daytime sky through Blender; seamless in longitude."""
import bpy
import numpy as np
from pathlib import Path
import json
OUT=Path('D:/projects/space/SourceArt/FrontEnd')
w,h=2048,1024
y=np.linspace(0,1,h)[:,None];x=np.linspace(0,2*np.pi,w,endpoint=False)[None,:]
altitude=np.minimum(1,np.abs(y-.5)*2)
horizon=np.array([.40,.69,.95]);zenith=np.array([.065,.34,.83])
arr=np.repeat(horizon[None,None,:]*(1-altitude[:,:,None]**.65)+zenith[None,None,:]*altitude[:,:,None]**.65,w,axis=1)
cloud=np.exp(-((np.abs(y-.5)-.16)/.035)**2)*np.maximum(0,np.sin(x*3+.6)+.45*np.sin(x*7-1)-.25)
cloud=np.clip(cloud*.23,0,.28)[:,:,None]
arr=arr*(1-cloud)+np.array([.88,.94,1.0])[None,None,:]*cloud
alpha=np.ones((h,w,1));pixels=np.concatenate([arr,alpha],axis=2).astype(np.float32)
im=bpy.data.images.new('T_FE_SkyGradient',width=w,height=h,alpha=True)
im.pixels.foreach_set(pixels.ravel());im.filepath_raw=str(OUT/'Textures/T_FE_SkyGradient.png');im.file_format='PNG';im.save();bpy.data.images.remove(im)
entries=[{'id':'T_FE_SkyGradient','source':'original_blender_procedural','file':'Textures/T_FE_SkyGradient.png','size':[w,h],'recipe':'Clear blue Earth daytime sky, quiet high white clouds; original procedural artwork'}]
(OUT/'sky_manifest.json').write_text(json.dumps(entries,indent=2),encoding='utf-8')
result={'sky':True}
