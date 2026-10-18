"""Shared authored thermal-field parameters for Blender and Unreal materials.

The angular texture is a connected, warped cellular network. Radius determines
travel up a rock from its inner root; the texture delays areas between channels.
It is sampled in undeformed planet space, so neither heat nor veins slide when
the rigid rocks move. This is an art-directed transport field, not a fluid sim.
"""
import math

BURN_KEYS = [(0,-.2),(150,-.12),(210,.04),(300,.22),(420,.43),
             (600,.70),(780,1.04),(870,1.12),(1200,1.12)]
RIFT_KEYS = [(0,0.),(24,0.),(36,.04),(60,.20),(105,.65),(150,1.),(1200,1.)]
OPENING_KEYS = [(0,0.),(30,0.),(45,.06),(75,.25),(120,.65),(180,1.),(1200,1.)]


def bake_flow(path):
    import bpy
    import numpy as np
    width,height=2048,1024
    image=bpy.data.images.new('T_MarsThermalFlow',width=width,height=height,alpha=True)
    image.colorspace_settings.name='Non-Color'
    pixels=np.ones((height,width,4),dtype=np.float32)

    def cellular(q):
        cell=np.floor(q)
        first=np.full(q.shape[:-1],100.,dtype=np.float32)
        second=first.copy()
        for x in (-1,0,1):
            for y in (-1,0,1):
                for z in (-1,0,1):
                    lattice=cell+np.array((x,y,z),dtype=np.float32)
                    h=lattice@np.array((127.1,311.7,74.7),dtype=np.float32)
                    random=np.stack([np.mod(np.sin(h+a)*43758.5453,1.) for a in (0.,19.19,47.77)],axis=-1)
                    delta=lattice+.15+.70*random-q
                    dist=np.sum(delta*delta,axis=-1)
                    second=np.minimum(second,np.maximum(first,dist))
                    first=np.minimum(first,dist)
        return np.sqrt(second)-np.sqrt(first)

    u=(np.arange(width,dtype=np.float32)+.5)/width
    for start in range(0,height,32):
        v=(np.arange(start,min(height,start+32),dtype=np.float32)+.5)/height
        theta=(u-.5)*2*math.pi
        phi=v[:,None]*math.pi
        direction=np.stack(np.broadcast_arrays(np.sin(phi)*np.cos(theta),
            np.sin(phi)*np.sin(theta),np.cos(phi)*np.ones_like(theta)),axis=-1)
        warp=.28*np.sin(direction[..., [1,2,0]]*7.1+direction[..., [2,0,1]]*3.7)
        coarse=cellular(direction*2.25+warp+7.13)
        fine=cellular(direction*6.+warp*1.7+13.6)
        noise=.5+.23*np.sin(direction@np.array((21.,11.,7.)))+.19*np.sin(direction@np.array((-13.,17.,29.)))
        # Secondary fingers only occur in selected patches. Fine branches are
        # transient flame fronts, never a permanent glowing contour everywhere.
        selected=np.clip((noise-.52)*7.,0,1)
        fine_delay=1.-selected*(1.-(.33+np.clip(fine*4.,0,1)*.67))
        channel=np.minimum(np.clip(coarse*3.4,0,1),fine_delay)
        pixels[start:start+len(v),:,0]=channel
        pixels[start:start+len(v),:,1]=np.clip(noise,0,1)
        pixels[start:start+len(v),:,2]=np.clip(coarse*5,0,1)
    image.pixels.foreach_set(pixels.ravel())
    image.filepath_raw=str(path)
    image.file_format='PNG'
    image.save()
    return image


def blender_flow(nodes,links,image,position):
    """Return angular channel and delay noise, using the same mapping as HLSL."""
    def math_node(op,a,b=None):
        n=nodes.new('ShaderNodeMath');n.operation=op
        for i,value in enumerate((a,b)):
            if value is None:continue
            if isinstance(value,(int,float)):n.inputs[i].default_value=value
            else:links.new(value,n.inputs[i])
        return n.outputs[0]
    normalize=nodes.new('ShaderNodeVectorMath');normalize.operation='NORMALIZE'
    links.new(position,normalize.inputs[0])
    xyz=nodes.new('ShaderNodeSeparateXYZ');links.new(normalize.outputs[0],xyz.inputs[0])
    u=math_node('ADD',math_node('MULTIPLY',math_node('ARCTAN2',xyz.outputs['Y'],xyz.outputs['X']),1/(2*math.pi)),.5)
    v=math_node('MULTIPLY',math_node('ARCCOSINE',xyz.outputs['Z']),1/math.pi)
    combine=nodes.new('ShaderNodeCombineXYZ');links.new(u,combine.inputs[0]);links.new(v,combine.inputs[1])
    texture=nodes.new('ShaderNodeTexImage');texture.image=image
    texture.interpolation='Linear'
    links.new(combine.outputs[0],texture.inputs[0])
    channels=nodes.new('ShaderNodeSeparateColor');links.new(texture.outputs[0],channels.inputs[0])
    return channels.outputs[0],channels.outputs[1],channels.outputs[2]


def rock_shader(material, texture, interior, pressure_keys):
    """Opaque rock with temporary hot fingers and sparse cooling reservoirs."""
    nodes,links=material.node_tree.nodes,material.node_tree.links
    shader=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
    shader.inputs['Roughness'].default_value=.86
    def calc(op,a,b=None):
        n=nodes.new('ShaderNodeMath');n.operation=op
        for i,v in enumerate((a,b)):
            if v is None:continue
            if isinstance(v,(int,float)):n.inputs[i].default_value=v
            else:links.new(v,n.inputs[i])
        return n.outputs[0]
    def clamp(v):return calc('MINIMUM',calc('MAXIMUM',v,0.),1.)
    def animated(name,keys):
        n=nodes.new('ShaderNodeValue');n.name=name
        for f,v in keys:
            n.outputs[0].default_value=v;n.outputs[0].keyframe_insert('default_value',frame=f)
        return n.outputs[0]
    import bpy
    rest=nodes.new('ShaderNodeAttribute');rest.attribute_name='RestPosition'
    flow,variation,coarse=blender_flow(nodes,links,bpy.data.images['T_MarsThermalFlow'],rest.outputs['Vector'])
    radius=nodes.new('ShaderNodeVectorMath');radius.operation='LENGTH';links.new(rest.outputs['Vector'],radius.inputs[0])
    r=radius.outputs['Value']
    uv=nodes.new('ShaderNodeUVMap');uv.uv_map='HeatField'
    separate=nodes.new('ShaderNodeSeparateXYZ');links.new(uv.outputs[0],separate.inputs[0])
    seam=calc('MAXIMUM',calc('SUBTRACT',1.,separate.outputs['Y']),0.)
    arrival=calc('ADD',.03,calc('MULTIPLY',r,.21))
    for value,factor in ((flow,.40),(clamp(calc('MULTIPLY',seam,1/.18)),.25),(variation,.06)):
        arrival=calc('ADD',arrival,calc('MULTIPLY',value,factor))
    time=animated('BurnProgress',BURN_KEYS)
    pressure=animated('InternalPressure',pressure_keys)
    animated('EdgeGlow',[(0,0.),(1200,0.)])
    rift=animated('RiftProgress',RIFT_KEYS)
    age=calc('SUBTRACT',time,arrival)
    burned=clamp(calc('MULTIPLY',age,1/.18))
    front=calc('POWER',clamp(calc('SUBTRACT',1.,calc('MULTIPLY',calc('ABSOLUTE',calc('SUBTRACT',age,.035)),10.))),2.)
    finger=clamp(calc('SUBTRACT',1.,calc('MULTIPLY',flow,1/.45)))
    wave=calc('ADD',calc('SUBTRACT',calc('MULTIPLY',r,32.),calc('MULTIPLY',time,65.)),calc('MULTIPLY',variation,13.))
    flicker=calc('ADD',.78,calc('MULTIPLY',calc('SINE',wave),.22))
    live=calc('MULTIPLY',front,calc('ADD',.5,calc('MULTIPLY',finger,.5)))
    hot=calc('MULTIPLY',calc('MULTIPLY',calc('POWER',live,3.),finger),flicker)
    residual=calc('MULTIPLY',burned,calc('POWER',clamp(calc('SUBTRACT',1.,calc('MULTIPLY',coarse,1/.50))),1.2))
    residual=calc('MULTIPLY',residual,clamp(calc('MULTIPLY',calc('SUBTRACT',variation,.55),4.)))
    groove=calc('MULTIPLY',rift,calc('POWER',clamp(calc('SUBTRACT',1.,calc('DIVIDE',seam,calc('ADD',.004,calc('MULTIPLY',rift,.024))))),.6)) if not interior else 0.
    root=calc('MULTIPLY',calc('MULTIPLY',calc('POWER',clamp(calc('SUBTRACT',1.,calc('MULTIPLY',seam,1/.006))),1.5),rift),calc('MINIMUM',calc('MULTIPLY',pressure,.3),1.)) if not interior else 0.
    base=nodes.new('ShaderNodeTexImage') if texture else nodes.new('ShaderNodeRGB')
    if texture:base.image=texture
    else:base.outputs[0].default_value=(.07,.026,.014,1)
    char=nodes.new('ShaderNodeMixRGB');links.new(burned,char.inputs[0]);links.new(base.outputs[0],char.inputs[1]);char.inputs[2].default_value=(.026,.012,.008,1)
    dark=nodes.new('ShaderNodeMixRGB');dark.blend_type='MULTIPLY';dark.inputs[0].default_value=1.;links.new(char.outputs[0],dark.inputs[1]);links.new(calc('SUBTRACT',1.,calc('MULTIPLY',groove,.85)),dark.inputs[2])
    links.new(dark.outputs[0],shader.inputs['Base Color'])
    emission=None
    for value,color,gain in ((hot,(1,.68,.25),5.),(live,(1,.14,.008),1.8),(residual,(1,.075,.006),2.),(calc('MULTIPLY',burned,calc('ADD',.4,calc('MULTIPLY',variation,.6))),(1,.025,.001),.035),(root,(1,.14,.008),.7)):
        n=nodes.new('ShaderNodeVectorMath');n.operation='SCALE';n.inputs[0].default_value=color
        links.new(calc('MULTIPLY',value,gain),n.inputs['Scale'])
        if emission is None:emission=n.outputs[0]
        else:
            add=nodes.new('ShaderNodeVectorMath');add.operation='ADD';links.new(emission,add.inputs[0]);links.new(n.outputs[0],add.inputs[1]);emission=add.outputs[0]
    if interior:
        deep=calc('MULTIPLY',calc('MULTIPLY',pressure,2.5),calc('POWER',clamp(calc('MULTIPLY',calc('SUBTRACT',1.02,r),1/.32)),.7))
        n=nodes.new('ShaderNodeVectorMath');n.operation='SCALE';n.inputs[0].default_value=(1,.42,.10);links.new(deep,n.inputs['Scale'])
        add=nodes.new('ShaderNodeVectorMath');add.operation='ADD';links.new(emission,add.inputs[0]);links.new(n.outputs[0],add.inputs[1]);emission=add.outputs[0]
    else:
        bump=nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.4;bump.inputs['Distance'].default_value=.025
        links.new(calc('SUBTRACT',1.,groove),bump.inputs['Height']);links.new(bump.outputs[0],shader.inputs['Normal'])
    links.new(emission,shader.inputs['Emission Color']);shader.inputs['Emission Strength'].default_value=1.
    return material
