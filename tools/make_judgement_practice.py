"""Attach an explicit RPA Gameplay V2 capsule to the generated v5 entity practice."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import zipfile
import hashlib

ROOT=Path(__file__).resolve().parents[1]
CAP="cuexis.gameplay.rpa.v1"


def write(path,value):
    path.write_text(json.dumps(value,indent=2)+"\n",encoding="utf-8")


def make_sources(directory,fixture):
    scene=json.loads((directory/"practice.v5.json").read_text())
    course=json.loads((directory/"course.json").read_text())["requirements"]
    source=json.loads((ROOT/"tests/fixtures/gameplay_author/inline_prototype.json").read_text())
    source["chartId"]=scene["chartId"]
    source["timing"]=scene["timing"]
    camera=copy.deepcopy(scene["camera"])
    camera["nearPlane"]=camera.pop("near");camera["farPlane"]=camera.pop("far")
    camera.update(pitch=0,yaw=0,roll=0)
    camera["defaultTransform"].update(rotation=[0,0,0,1],scale=[1,1,1])
    source["defaultCamera"]=camera
    source.pop("mainMusic",None)
    source["entities"]=[]
    closure={}
    for obj in scene["objects"]:
        c=obj["components"];t=c["cuexis.transform"];r=c["cuexis.renderable"]
        components=[dict(kind="transform",position=t["position"],rotation=t["rotation"],scale=t["scale"]),
                    dict(kind="renderable",mesh=r["mesh"]["id"],material=r["material"]["id"],alpha=r["alpha"])]
        source["entities"].append(dict(identity=dict(kind="explicit",objectId=obj["id"]),
                                       components=components,requirements=[]))
        closure[r["mesh"]["id"]]="renderableMesh";closure[r["material"]["id"]]="renderableMaterial"
    source["resourceClosure"]=[dict(assetId=k,use=v) for k,v in sorted(closure.items())]
    game=source["gameplay"];common=game["common"]
    template=copy.deepcopy(game["requirements"][0]);grace=copy.deepcopy(common["graceInputs"][0]["inputs"])
    common["executionProfile"]="gameplay.execution.t4-k4.rpa.v1"
    common["declaredCapabilities"]=[dict(capabilityId=CAP,revision="1")]
    common["latePolicy"]=dict(mode="queue_next_tick",**{k:dict(state="measured",value=str(v))
        for k,v in [("finalizationWatermark",100000),("maxQueueHop",50000),
                    ("windowCloseThreshold",40000),("windowOpenThreshold",20000)]})

    common["graceInputs"]=[];common["factBindings"]=[]
    common["declarations"]=[d for d in common["declarations"] if d["kind"]!="requirement"]
    game["requirements"]=[];game["rankAssignment"]["rows"]=[]
    config=json.loads((fixture/"configuration.json").read_text());c=config["configuration"]
    c["engine"][1]="fact.semantic.rpa.v1";c["engine"][4]=common["executionProfile"]
    c["engine"][5]="late.window.batch.v1";c["enabledCapabilities"]=[CAP]
    c["calibration"]=json.loads((directory/"compile.log").read_text())["calibration"]
    c["bindings"]=[]
    c["scoreRules"]=[dict(phase=phase,outcome=outcome,grade=None,
        delta=delta if outcome=="hit" else -1,incrementsCombo=outcome=="hit")
        for phase,delta in [("tap",2),("head",2),("body",3),("tail",5)] for outcome in ["hit","miss"]]
    for ordinal,item in enumerate(course,4):
        r=copy.deepcopy(template);name=item["id"];head=item["headUs"];tail=item["tailUs"]
        r["stableId"]["declarationOrdinal"]=ordinal
        r["identity"]["requirementLocalId"]=name
        r["identity"]["emissionPath"]=[dict(nodeId=name,repeatIndex="0")]
        r["factBindingRefs"]=[]
        r["requiredRefs"]["capabilities"]=[dict(capabilityId=CAP,revision="1")]
        r["atomBindings"][0]["channelToken"]="lane."+item["key"].lower()
        phases=["head","body","tail"] if tail else ["tap"]
        r["phases"]=[dict(kind=p,declarationOrdinal=n) for n,p in enumerate(phases,1)]
        r["requiresReleaseTailSemantics"]=bool(tail)
        r["measure"]["components"]=[dict(phase=p,categoryToken="hold_"+p if tail else "tap",gradeTokens=[]) for p in phases]
        if tail:
            r["timing"]=dict(end=str(tail+120000),successWindows=[
                dict(phase=r["phases"][0],start=str(head-120000),end=str(head+120000)),
                dict(phase=r["phases"][1],start=str(head+120000),end=str(tail-120000)),
                dict(phase=r["phases"][2],start=str(tail-120000),end=str(tail+120000))],
                body=dict(start=str(head+120000),end=str(tail-120000)),
                phaseTargets=[dict(phase=p,chartTick=str(t)) for p,t in [("head",head),("body",tail-120000),("tail",tail)]])
            release=copy.deepcopy(r["atomBindings"][0]);release.update(atomRef="tail.release",action="release",tailOnly=True)
            r["atomBindings"].append(release)
        else:
            r["timing"]=dict(end=str(head+120000),successWindows=[dict(phase=r["phases"][0],start=str(head-120000),end=str(head+120000))],phaseTargets=[dict(phase="tap",chartTick=str(head))])
        common["declarations"].append(dict(stableId=r["stableId"],kind="requirement",localName=name,references=[],requiredRefs=dict(capabilities=[],features=[])))
        gi=copy.deepcopy(grace);gi["maximumCanonical"]="40000000"
        common["graceInputs"].append(dict(requirement=r["stableId"],inputs=gi))
        game["requirements"].append(r)
        game["rankAssignment"]["rows"].append(dict(identity=r["identity"],priority="-1",tieRank=str(ordinal),namespace="@independent"))
        # The registered author dialect owns Requirements through genuine generated emissions.
        # Visual head Objects remain separate; FactBindings target explicit scene feedback Objects.
        source["entities"].append(dict(identity=dict(kind="generated",chartId=source["chartId"],
            bindingId=r["identity"]["invocationId"],moduleId=r["identity"]["moduleId"],
            exportId=r["identity"]["exportId"],path=[dict(nodeId=name,iterationIndexPlusOne=0)]),
            components=[],requirements=[]))
        identity=copy.deepcopy(r["identity"])
        for step in identity["emissionPath"]:step["repeatIndex"]=int(step["repeatIndex"])
        for phase in phases:
            for outcome in ["hit","miss"]:
                binding=name+"."+phase+"."+outcome
                common["factBindings"].append(binding);r["factBindingRefs"].append(binding)
                c["bindings"].append(dict(bindingId=binding,source=identity,phase=phase,outcome=outcome,timing="any",
                    target=item["feedback"][phase][outcome],visible=True,start=0,end=None,aggregation="any",groupMembers=[]))
    return source,config


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory",type=Path,required=True)
    parser.add_argument("--fixture",type=Path,required=True)
    parser.add_argument("--assembler",type=Path,required=True)
    args=parser.parse_args();directory=args.directory.resolve()
    source,config=make_sources(directory,args.fixture.resolve())
    write(directory/"gameplay-author.v5.json",source);write(directory/"gameplay-configuration.json",config)
    def package_bytes(value):
        return (json.dumps(value,indent=2)+"\n").encode()
    def pack(path,entries,extensions):
        manifest=dict(format="cuexis.cxc",version=1,project="cuexis.project.json",
            entries=[dict(path=k,byteCount=len(v),sha256=hashlib.sha256(v).hexdigest()) for k,v in sorted(entries.items())],
            requiredExtensions=[],extensions=extensions)
        with zipfile.ZipFile(path,"w",compression=zipfile.ZIP_STORED) as z:
            for k,v in [("cuexis.cxc.json",package_bytes(manifest)),*sorted(entries.items())]:
                z.writestr(zipfile.ZipInfo(k,(1980,1,1,0,0,0)),v)
    with zipfile.ZipFile(directory/"practice.cxc") as z:
        scene_entries={n:z.read(n) for n in z.namelist() if n!="cuexis.cxc.json"}
        scene_ext=json.loads(z.read("cuexis.cxc.json"))["extensions"]["cuexis.chart-entry.v1"]
    base_entries={k:v for k,v in scene_entries.items() if k!="compiled/chart.v5.json"}
    project=json.loads(base_entries["cuexis.project.json"]);project["extensions"]={}
    base_entries["cuexis.project.json"]=package_bytes(project)
    pack(directory/"gameplay-base.cxc",base_entries,{})
    command=[str(args.assembler.resolve()),"--base",str(directory/"gameplay-base.cxc"),"--foundation",str(directory/"gameplay-foundation.packed"),
        "--configuration",str(directory/"gameplay-configuration.json"),"--entry-id","chart.entry.one","--test-only","true",
        "--configuration-budget","131072,64,8192,16384,8192","--graph-budget","16777216,64,8192,40000,40000,1048576,1048576",
        "--source",str(directory/"gameplay-author.v5.json"),"--source-kind","inline","--publish","cxc","--output",str(directory/"practice.judgement.cxc")]
    result=subprocess.run(command,capture_output=True,timeout=120)
    (directory/"gameplay-assemble.log").write_bytes(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError("Gameplay assembly failed; see "+str(directory/"gameplay-assemble.log"))
    with zipfile.ZipFile(directory/"practice.judgement.cxc") as package:
        assembled={n:package.read(n) for n in package.namelist() if n!="cuexis.cxc.json"}
        extension=json.loads(package.read("cuexis.cxc.json"))["extensions"]
    assembled["compiled/chart.v5.json"]=scene_entries["compiled/chart.v5.json"]
    project=json.loads(assembled["cuexis.project.json"])
    project["extensions"]["cuexis.chart-entry.v1"]=scene_ext
    assembled["cuexis.project.json"]=package_bytes(project)
    extension["cuexis.chart-entry.v1"]=scene_ext
    pack(directory/"practice.judgement.cxc",assembled,extension)
    (directory/"gameplay.packed").write_bytes(assembled["compiled/gameplay.packed"])
    course=json.loads((directory/"course.json").read_text())["requirements"]
    def inputs(early=False):
        events=[]
        for item in course:
            head,tail=item["headUs"],item["tailUs"]
            channel="lane."+item["key"].lower()
            release=(head+100000 if early else tail) if tail else head+30000
            events.extend([dict(tick=head,action="press",channel=channel),
                           dict(tick=release,action="release",channel=channel)])
        return sorted(events,key=lambda event:event["tick"])
    write(directory/"verification-cases.json",[
        dict(name="all-hit",end=35000000,inputs=inputs(),score=46,hits=21,misses=0),
        dict(name="no-input",end=35000000,inputs=[],score=-21,hits=0,misses=21),
        dict(name="early-hold-release",end=35000000,inputs=inputs(True),score=36,hits=19,misses=2)])
    print("Attached 19 Requirements / 21 phases with actual RPA Gameplay V2")


if __name__=="__main__":main()


