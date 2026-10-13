"""Build a self-contained v5 Behavior candidate entity practice; no Player-specific guide."""
import argparse
from fractions import Fraction
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import subprocess
import wave
import zipfile

COURSE = [("D", 3), ("F", 4), ("J", 5), ("K", 6), ("D", 7), ("F", 7.5),
          ("J", 8), ("K", 8.5), ("D", 10), ("J", 10), ("F", 11), ("K", 11),
          ("K", 13), ("D", 20), ("F", 22), ("J", 24), ("K", 26), ("D", 31.5), ("J", 32.5)]
FONT = {
    "D": [30,17,17,17,17,17,30], "F": [31,16,16,30,16,16,16],
    "J": [7,2,2,2,18,18,12], "K": [17,18,20,24,20,18,17],
    "T": [31,4,4,4,4,4,4], "E": [31,16,16,30,16,16,31],
    "S": [15,16,16,14,1,1,30], "P": [30,17,17,30,16,16,16],
    "A": [14,17,17,31,17,17,17], "C": [14,17,16,16,16,17,14],
    "R": [30,17,17,30,20,18,17], "U": [17,17,17,17,17,17,14],
    " ": [0]*7, "/": [1,1,2,4,8,16,16],
    "V": [17,17,17,17,17,10,4], "5": [31,16,16,30,1,1,30],
    "I": [31,4,4,4,4,4,31],
    "0": [14,17,19,21,25,17,14], "1": [4,12,4,4,4,4,14],
    "2": [14,17,1,2,4,8,31], "3": [30,1,1,14,1,1,30],
    "4": [2,6,10,18,31,2,2], "6": [14,16,16,30,17,17,14],
    "7": [31,1,2,4,8,8,8], "8": [14,17,17,14,17,17,14],
    "9": [14,17,17,15,1,1,14], "H": [17,17,17,31,17,17,17],
    "B": [30,17,17,30,17,17,30], "L": [16,16,16,16,16,16,31],
}


def encoded(value):
    return (json.dumps(value, ensure_ascii=True, indent=2) + "\n").encode()


def beat(seconds):
    # This authored course uses quarter/half/tenth seconds; remove arithmetic float noise only.
    exact = Fraction(str(seconds)).limit_denominator(1000)
    assert abs(float(exact) - float(seconds)) < 1e-12
    q = exact * 2
    return {"numerator": q.numerator, "denominator": q.denominator}


def mesh(rectangles):
    vertices, indices = [], []
    for x, y, w, h in rectangles:
        offset = len(vertices) // 3
        vertices += [x,y,0, x+w,y,0, x+w,y+h,0, x,y+h,0]
        indices += [offset+i for i in [0,1,2,0,2,3]]
    size = 40 + len(vertices)*4 + len(indices)*4
    return (struct.pack("<8sIIQIIII", b"CXPRES01",1,1,size,len(vertices)//3,len(indices),0,0)
            + struct.pack(f"<{len(vertices)}f", *vertices)
            + struct.pack(f"<{len(indices)}I", *indices))


def text_mesh(text):
    return mesh([(column*6+x, 6-y, .85, .85)
                 for column, letter in enumerate(text)
                 for y, row in enumerate(FONT[letter]) for x in range(5) if row & (1 << (4-x))])


def material(color):
    return struct.pack("<8sIIQII4fII", b"CXPRES01",3,1,56,1,1,*color,1,0,0)


def sound():
    rate = 48000
    # 32 seconds of cues plus three explicitly silent seconds for the presentation preview.
    samples = bytearray(rate*35*2)
    pulses = [(i*.5, 880 if i%4==0 else 660, 2500) for i in range(64)]
    pulses += [(t, {"D":1047,"F":1175,"J":1319,"K":1568}[key],4200)
               for key,t in COURSE if t<32]
    pulses.append((17,1760,4200))
    for t, frequency, gain in pulses:
        start = round(t*rate)
        for i in range(rate//20):
            offset = (start+i)*2
            old = struct.unpack_from("<h",samples,offset)[0]
            value = round(gain*(1-i/(rate/20))**2*math.sin(2*math.pi*frequency*i/rate))
            struct.pack_into("<h",samples,offset,max(-32768,min(32767,old+value)))
    out = io.BytesIO()
    with wave.open(out,"wb") as w:
        w.setparams((1,2,rate,0,"NONE","not compressed")); w.writeframes(samples)
    return out.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--compiler",type=Path,required=True)
    parser.add_argument("--fixture",type=Path,required=True)
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    entries, assets, objects, behaviors = {}, [], [], []
    def asset(id, kind, path, data):
        entries["assets/"+path] = data
        assets.append(dict(id=id,type=kind,source=path,dependencies=[]))
    asset("mesh.quad","mesh","meshes/quad.mesh.bin",mesh([(-.5,-.5,1,1)]))
    for name,color in [("D",(.2,.8,1)),("F",(.3,1,.65)),("J",(1,.75,.25)),
                       ("K",(.8,.5,1)),("miss",(1,.25,.25)),("white",(.85,.9,1)),("grey",(.13,.17,.24))]:
        asset("material."+name,"material",f"materials/{name}.material.bin",material(color))
    for name,text in [("title","DFJK TEST"),("instructions","SPACE PAUSE / R RESTART"),
                      *[(key,key) for key in "DFJK"]]:
        asset("mesh.text."+name,"mesh",f"meshes/text-{name}.mesh.bin",text_mesh(text))
    asset("audio.main","audio","audio/practice.wav",sound())
    def entity(name, position, scale, mat, geometry="mesh.quad", behavior=None, note=None):
        id = f"019f0000-0000-7abc-8def-{len(objects)+1:012d}"
        components = {"cuexis.transform":dict(version=1,position=position,rotation=[0,0,0,1],scale=scale),
                      "cuexis.renderable":dict(version=1,mesh=dict(domain="asset",id=geometry),
                                               material=dict(domain="asset",id="material."+mat))}
        if behavior: components["cuexis.behavior"] = dict(version=1,behavior=dict(domain="behavior",id=behavior))
        if note is not None: components["cuexis.note"] = dict(version=1,beat=beat(note))
        else: components["cuexis.element"] = dict(version=1)
        objects.append(dict(id=id,name=name,parent=None,components=components,extensions={}))
        return id
    entity("title_dfjk_test",[-1.7,2.15,.01],[.07,.07,1],"white","mesh.text.title")
    entity("instructions_pause_restart",[-2.2,1.83,.01],[.04,.04,1],"white","mesh.text.instructions")
    for lane,key in enumerate("DFJK"):
        x=lane-1.5
        entity(f"track_{key}_background",[x,.15,-.04],[.85,2.9,1],"grey")
        entity(f"track_{key}_left",[x-.425,.15,-.01],[.012,2.9,1],key)
        entity(f"track_{key}_right",[x+.425,.15,-.01],[.012,2.9,1],key)
        entity(f"key_label_{key}",[x-.16,-1.85,.01],[.065,.065,1],key,"mesh.text."+key)
    entity("judgement_line",[0,-1.2,.02],[4.3,.025,1],"white")
    def falling(name,key,target,width,height,center_shift=0):
        start=max(0,target-1.35)
        finish=target+.12
        y=-1.2+2*(target-start)+center_shift
        behavior="behavior."+name
        steps=[dict(property="render.visible",beat=beat(0),value=False),
               dict(property="render.visible",beat=beat(start),value=True),
               dict(property="render.visible",beat=beat(finish),value=False)]
        behaviors.append(dict(id=behavior,type="behavior.event",version=1,
            events=[dict(property="transform.position.y",startBeat=beat(start),
                         durationBeats=beat(finish-start),startValue=y,
                         endValue=-1.44+center_shift,startSlope=1,endSlope=1)],stepEvents=steps))
        entity(name,["DFJK".index(key)-1.5,y,.03],[width,height,1],key,behavior=behavior,note=target)
    for index,(key,target) in enumerate(COURSE):
        falling(f"note_{index+1:02d}_{key}",key,target,.65,.13)
    body_behavior="behavior.hold_K_body"
    body_events=[]
    for start,y0,y1,h0,h1 in [(11.65,1.48,.15,.04,2.7),(15.65,.15,-1.18,2.7,.04)]:
        for prop,a,b in [("transform.position.y",y0,y1),
                         ("transform.scale",[.18,h0,1],[.18,h1,1])]:
            body_events.append(dict(property=prop,startBeat=beat(start),durationBeats=beat(1.35),
                                    startValue=a,endValue=b,startSlope=1,endSlope=1))
    behaviors.append(dict(id=body_behavior,type="behavior.event",version=1,events=body_events,
        stepEvents=[dict(property="render.visible",beat=beat(t),value=visible)
                    for t,visible in [(0,False),(11.65,True),(17.12,False)]]))
    entity("hold_K_body",[1.5,1.48,.025],[.18,.04,1],"K",behavior=body_behavior)
    falling("hold_K_tail","K",17,.65,.13)
    # Feedback is chart content; only genuine FactBinding outcomes reveal these badges.
    hidden_behavior="behavior.feedback.initially_hidden"
    behaviors.append(dict(id=hidden_behavior,type="behavior.event",version=1,events=[],
        stepEvents=[dict(property="render.visible",beat=beat(0),value=False)]))
    asset("mesh.text.results","mesh","meshes/text-results.mesh.bin",text_mesh("RESULTS"))
    entity("results_title",[2.35,1.35,.01],[.035,.035,1],"white","mesh.text.results")
    course=[]
    row=0
    for index,(key,target) in enumerate(COURSE,1):
        name=f"note_{index:02d}_{key}"
        is_hold=key=="K" and target==13
        phases=["head","body","tail"] if is_hold else ["tap"]
        item=dict(id=name,key=key,kind="hold" if is_hold else "tap",headUs=round(target*1000000),
                  tailUs=17000000 if is_hold else 0,
                  visualHeadObjectId=next(o["id"] for o in objects if o["name"]==name),feedback={})
        for phase in phases:
            column,local_row=divmod(row,7)
            x,y=2.30+column*.90,1.00-local_row*.38
            label=f"{index:02d}"+(phase[0].upper() if is_hold else key)
            mesh_id="mesh.text.result."+str(row)
            asset(mesh_id,"mesh",f"meshes/text-result-{row}.mesh.bin",text_mesh(label))
            entity("result_label_"+str(row),[x,y,.01],[.025,.025,1],"white",mesh_id)
            item["feedback"][phase]={}
            for outcome,mat in [("hit","F"),("miss","miss")]:
                item["feedback"][phase][outcome]=entity(f"result_{name}_{phase}_{outcome}",
                    [x+.58,y-.07,.04],[.15,.15,1],mat,behavior=hidden_behavior)
            row+=1
        course.append(item)
    (out/"course.json").write_bytes(encoded(dict(timebase="us",requirements=course,
        expectedAllHit=dict(score=46,hits=21,misses=0),expectedNoInput=dict(score=-21,hits=0,misses=21))))
    chart=dict(format="cuexis.chart",version=4,chartId="019f0000-0000-7abc-8def-000000009001",
        metadata=dict(title="DFJK Entity Practice",artist="Program generated"),
        timing=dict(offsetMs=0,defaultBpm=120,tempoEvents=[],stops=[]),
        camera=dict(type="perspective",fovY=60,near=.1,far=1000,
                    defaultTransform=dict(position=[0,0,5])),
        audio=dict(version=1,mainMusic=dict(domain="asset",id="audio.main")),parameters=[],
        templates=[],behaviors=behaviors,animationTemplateImports=[],animationClips=[],
        objects=objects,requiredExtensions=[],extensions={})
    # Explicit v5 Behavior source candidate; the existing static Packed profile is unchanged.
    chart["version"]=5
    for behavior in chart["behaviors"]: behavior["version"]=2
    for obj in chart["objects"]: obj["components"]["cuexis.renderable"]["alpha"]=255
    labels={obj["name"]:obj["id"] for obj in chart["objects"]}
    source_path=out/"practice.v5.json"; source_path.write_bytes(encoded(chart))
    compiled=out/"chart.v5.json"
    command=[str(args.compiler.resolve()),str(source_path),str((args.fixture/"configuration.json").resolve()),
             str((args.fixture/"music-foundation.packed").resolve()),str(compiled),str(out/"gameplay-foundation.packed")]
    result=subprocess.run(command,capture_output=True,timeout=120)
    (out/"compile.log").write_bytes(result.stdout+result.stderr)
    if result.returncode: raise RuntimeError(f"V5 fixture compilation failed: {out/'compile.log'}")
    report=json.loads(result.stdout)
    assert report["entities"]==len(objects) and report["requirements"]==0 and report["roundTrip"]
    payload=compiled.read_bytes()
    entry=dict(path="compiled/chart.v5.json",kind="chart",encoding="source-json",playback=True,
        compiledSemanticIdentity=report["semanticIdentity"],
        artifactIdentity=hashlib.sha256(payload).hexdigest(),compilerProfile="candidate.behavior-v2-v1",
        expandedEntityCount=len(objects),expandedRequirementCount=0)
    extension={"cuexis.chart-entry.v1":dict(entries=[entry])}
    entries["compiled/chart.v5.json"]=payload
    (out/"entity-index.json").write_bytes(encoded(labels))
    chart["version"]=4
    chart["objects"]=[]; chart["behaviors"]=[]
    chart["metadata"]["title"]="Compatibility fallback, not the v5 test entry"
    entries["assets/charts/compatibility-fallback.cuexis.chart.json"]=encoded(chart)
    entries["assets/cuexis.asset-index.json"]=encoded(dict(format="cuexis.asset-index",version=2,assets=assets,extensions={}))
    entries["cuexis.project.json"]=encoded(dict(format="cuexis.project",version=1,
        projectId="019f0000-0000-7abc-8def-000000009000",assetRoots=[dict(id="main",path="assets")],
        entry=dict(chart=dict(root="main",path="charts/compatibility-fallback.cuexis.chart.json")),extensions=extension))
    manifest=dict(format="cuexis.cxc",version=1,project="cuexis.project.json",requiredExtensions=[],extensions=extension,
        entries=[dict(path=name,byteCount=len(data),sha256=hashlib.sha256(data).hexdigest()) for name,data in sorted(entries.items())])
    with zipfile.ZipFile(out/"practice.cxc","w",compression=zipfile.ZIP_STORED) as z:
        for name,data in [("cuexis.cxc.json",encoded(manifest)),*sorted(entries.items())]:
            z.writestr(zipfile.ZipInfo(name,(1980,1,1,0,0,0)),data)
    for name,data in entries.items():
        path=out/"source"/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
    (out/"manifest.json").write_bytes(encoded(manifest))
    print(f"Generated {len(labels)} v5 candidate entities, {report['behaviors']} executable behaviors, {len(assets)} assets: {out}")


if __name__=="__main__":
    main()
