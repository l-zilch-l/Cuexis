"""Generate the private candidate payload schema from the selected author leaf grammar."""

import json
from pathlib import Path


def schema():
    text = {"type": "string", "minLength": 1}
    literal = {"type": "string"}
    i64 = {"type": "string", "pattern": r"^(0|[1-9][0-9]*|-[1-9][0-9]*)$", "maxLength": 20}
    u64 = {"type": "string", "pattern": r"^(0|[1-9][0-9]*)$", "maxLength": 20}
    u32 = {"type": "integer", "minimum": 0, "maximum": 4294967295}
    boolean = {"type": "boolean"}
    defs = {}

    def ref(name):
        return {"$ref": "#/definitions/" + name}

    def array(item):
        return {"type": "array", "items": item}

    def enum(*values):
        return {"type": "string", "enum": list(values)}

    def obj(required, optional=None):
        return {"type": "object", "additionalProperties": False,
                "required": list(required), "properties": {**required, **(optional or {})}}

    phase_kind = enum("tap", "head", "body", "tail")
    defs["Q"] = obj({"numerator": i64, "denominator": {**u64, "not": {"const": "0"}}})
    defs["StableId"] = obj({"sourceDocumentId": text, "declarationOrdinal": u32})
    defs["Phase"] = obj({"kind": phase_kind, "declarationOrdinal": u32})
    defs["Identity"] = obj({**{k: text for k in ("chartEntryId", "invocationId", "moduleId", "exportId", "requirementLocalId")},
                            "emissionPath": array(obj({"nodeId": text, "repeatIndex": u64}))})
    capability = obj({"capabilityId": text}, {"revision": text})
    defs["RequiredRefs"] = obj({"features": array(text), "capabilities": array(capability)})
    refs = ref("RequiredRefs")
    defs["Node"] = {"oneOf": [
        obj({"primitive": {"const": "atom"}, "operands": {**array(ref("Node")), "maxItems": 0}, "atomRef": text}),
        obj({"primitive": enum("sequence", "choice", "complement", "skip", "instant"), "operands": array(ref("Node"))}),
        obj({"primitive": {"const": "boundedRepeat"}, "operands": array(ref("Node")),
             "repeatBounds": obj({"minimum": u64, "maximum": u64})})]}
    defs["Pattern"] = obj({"matchPolicy": {"const": "leftmost-first"}, "root": ref("Node"), "requiredRefs": refs}, {"patternId": text})
    defs["Measure"] = obj({"components": array(obj({"phase": phase_kind, "categoryToken": literal, "gradeTokens": array(text)})), "requiredRefs": refs})
    defs["Grace"] = {"oneOf": [
        obj({"policy": enum("explicit", "default"), "allowChartGrace": boolean}),
        obj({"policy": {"const": "inherited"}, "allowChartGrace": boolean, "inheritedFromDeclarationId": text})]}
    defs["GraceInputs"] = obj({"unitInTicks": ref("Q"), "minimumCanonical": i64, "maximumCanonical": i64},
                              {k: ref("Q") for k in ("chartDuration", "inheritedDuration", "defaultDuration")})
    interval = obj({"start": i64, "end": i64})
    defs["Timing"] = obj({"end": i64, "successWindows": array(obj({"phase": ref("Phase"), "start": i64, "end": i64})),
                          "phaseTargets": array(obj({"phase": phase_kind, "chartTick": i64}))}, {"body": interval})
    defs["AtomBinding"] = obj({**{k: text for k in ("atomRef", "domainToken", "sourceClass", "channelToken")},
                               "action": enum("press", "release", "update"), "tailOnly": boolean},
                              {"amountRange": obj({"minimum": i64, "maximum": i64})})
    claim = obj({"resourceId": text, "intent": enum("observe", "consume", "claim"), "policyToken": text,
                 "graceOverride": obj({"mode": {"const": "none"}, "overrideToken": {"const": ""}})})
    measured_u64 = obj({"state": {"const": "measured"}, "value": u64})
    defs["Requirement"] = obj({
        "stableId": ref("StableId"), "identity": ref("Identity"), "requiredActions": array(text),
        "domainBinding": text, "judgementDomainId": text, "phases": array(ref("Phase")),
        "requiresReleaseTailSemantics": boolean, "pattern": ref("Pattern"), "patternArmRefs": array(text),
        "maxArmElements": measured_u64, "maxDeadlineElements": measured_u64, "measure": ref("Measure"),
        "resourceClaims": {**array(claim), "maxItems": 1}, "grace": ref("Grace"), "timing": ref("Timing"),
        "atomBindings": array(ref("AtomBinding")), "solverProfileRef": text, "localClosePolicyToken": text,
        "factBindingRefs": array(text), "requiredRefs": refs})
    declaration_ref = {"oneOf": [
        obj({"scope": {"const": "sameDocument"}, "declarationOrdinal": u32}),
        obj({"scope": {"const": "explicitCrossDocument"}, "sourceDocumentId": text, "declarationOrdinal": u32})]}
    defs["Declaration"] = obj({"stableId": ref("StableId"), "kind": enum("requirement", "patternDefinition", "measureDefinition", "resourceRecord", "judgementDomain", "solverProfile"),
                               "localName": text, "references": array(declaration_ref), "requiredRefs": refs})
    defs["Resource"] = obj({"resourceId": text, "declaredCapacity": u64, "slotToken": text, "decisionPolicyRef": text,
                            "terminalAfterTermination": boolean, "declaredGapGrace": i64, "requiredRefs": refs})
    defs["Relation"] = obj({"kind": {"const": "exclusive"}, "resourceId": text, "members": array(ref("StableId")),
                            "policyToken": text, "declaredCapacity": u64, "requiredRefs": refs})
    defs["Solver"] = obj({"solverId": text, "revision": text, "algorithmToken": text, "objective": array(text),
                          "tieBreak": array(text), "rejectIfNonUnique": boolean, "requiredRefs": refs})
    defs["Domain"] = obj({"domainId": text, "coordinateSystemToken": text,
                          "axes": array(obj({"axisToken": text, "minimum": i64, "maximum": i64})),
                          "frame": {"const": "static"}, "requiredRefs": refs})
    defs["Timebase"] = obj({"profileId": text, "unitToken": text, "tickScale": ref("Q"), "originBeat": ref("Q"), "initialTempo": ref("Q"),
                            "tempoSections": array(obj({"startBeat": ref("Q"), "durationPerBeat": ref("Q")})),
                            "stopSections": array(obj({"startBeat": ref("Q"), "endBeat": ref("Q"), "duration": ref("Q")}))})
    defs["Late"] = obj({"mode": enum("reject_late", "queue_next_tick"),
                        **{k: obj({"state": {"const": "measured"}, "value": i64}) for k in
                           ("finalizationWatermark", "maxQueueHop", "windowCloseThreshold", "windowOpenThreshold")}})
    defs["MeasureDefinition"] = obj({"declaration": ref("Measure")}, {"id": text})
    defs["Common"] = obj({"chartEntryId": text, "graphRevision": {"const": "2"}, "rulesetRef": text,
                          "normalizationProfileToken": text, "coordinatorPolicy": {"const": "coordinator.policy.greedy_v1"},
                          "executionProfile": {"const": "gameplay.execution.t4-k4.v1"}, "timebase": ref("Timebase"), "latePolicy": ref("Late"),
                          "graceInputs": array(obj({"requirement": ref("StableId"), "inputs": ref("GraceInputs")})),
                          "declarations": array(ref("Declaration")), "resources": array(ref("Resource")), "relations": array(ref("Relation")),
                          "solverProfiles": array(ref("Solver")), "factBindings": array(text), "judgementDomains": array(ref("Domain")),
                          "declaredFeatures": array(text), "declaredCapabilities": array(capability),
                          "namedGraceDurations": array(obj({"declarationId": text, "duration": ref("Q")})),
                          "patternDefinitions": array(ref("Pattern")), "measureDefinitions": array(ref("MeasureDefinition"))})
    explicit_row = obj({"identity": ref("Identity"), "priority": i64, "tieRank": u64, "namespace": text})
    block = obj({**{k: text for k in ("invocationId", "moduleId", "exportId", "requirementLocalId", "namespace")},
                 "priority": i64, "base": u64, "stride": {**u64, "not": {"const": "0"}},
                 "nodeOrder": array(text), "radices": array(u64)})
    defs["RankAssignment"] = {"oneOf": [obj({"mode": {"const": "explicit"}, "rows": array(explicit_row)}),
                                          obj({"mode": {"const": "affine"}, "blocks": array(block)})]}
    return {"$schema": "http://json-schema.org/draft-07/schema#", "$id": "cuexis.gameplay-author.t4-k4.v1",
            "title": "Private candidate Gameplay T4/K4 author payload", **obj({"version": {"const": 2},
            "authorProfile": {"const": "gameplay.author.t4-k4.v1"}, "sourceDocumentId": text,
            "common": ref("Common"), "requirements": array(ref("Requirement")), "rankAssignment": ref("RankAssignment")}),
            "definitions": defs}


def inline_schema():
    def ref(name): return {"$ref": "#/definitions/" + name}
    def array(item): return {"type": "array", "items": item}
    def enum(*values): return {"type": "string", "enum": list(values)}
    def obj(required, optional=None):
        return {"type": "object", "additionalProperties": False, "required": list(required), "properties": {**required, **(optional or {})}}
    text = {"type": "string", "minLength": 1}
    u32 = {"type": "integer", "minimum": 0, "maximum": 4294967295}
    payload = schema()
    defs = payload.pop("definitions")
    payload.pop("$id")
    payload.pop("$schema")
    defs["Payload"] = payload
    num = {"type": "number"}
    vector = lambda n: {"type": "array", "items": num, "minItems": n, "maxItems": n}
    transform = {"position": vector(3), "rotation": vector(4), "scale": vector(3)}
    camera = {"type": text, "fovY": num, "nearPlane": num, "farPlane": num}
    defs["Transform"] = obj(transform)
    defs["EntityIdentity"] = {"oneOf": [obj({"kind": {"const": "explicit"}, "objectId": text}),
        obj({"kind": {"const": "generated"}, **{k: text for k in ("chartId", "bindingId", "moduleId", "exportId")},
        "path": array(obj({"nodeId": text, "iterationIndexPlusOne": u32}))})]}
    defs["Component"] = {"oneOf": [obj({"kind": {"const": "transform"}, **transform}),
        obj({"kind": {"const": "renderable"}, "mesh": text, "material": text, "alpha": {"type": "integer", "minimum": 0, "maximum": 255}}),
        obj({"kind": {"const": "camera"}, **camera})]}
    return {"$schema": "http://json-schema.org/draft-07/schema#", "$id": "cuexis.gameplay-inline.t4-k4.v1",
        **obj({"format": {"const": "cuexis.chart"}, "version": {"const": 5}, "chartId": text,
        "features": array(obj({"id": text, "version": u32})),
        "timing": obj({"offsetMs": num, "defaultBpm": num,
            "tempoEvents": array(obj({"startBeat": ref("Q"), "durationBeats": ref("Q"), **{k: num for k in ("startBpm", "endBpm", "startSlope", "endSlope")}})),
            "stops": array(obj({"beat": ref("Q"), "durationMs": num}))}),
        "defaultCamera": obj({**camera, **{k: num for k in ("pitch", "yaw", "roll")}}, {"defaultTransform": ref("Transform")}),
        "resourceClosure": array(obj({"assetId": text, "use": enum("mainMusic", "renderableMesh", "renderableMaterial")})),
        "entities": array(obj({"identity": ref("EntityIdentity"), "components": array(ref("Component")), "requirements": {"type": "array", "maxItems": 0}}, {"parent": ref("EntityIdentity")})),
        "requiredExtensions": {"const": ["cuexis.gameplay.v2"]}, "gameplay": ref("Payload")}, {"mainMusic": text}), "definitions": defs}


if __name__ == "__main__":
    target = Path(__file__).resolve().parents[2] / "schemas/candidate/cuexis.gameplay-author.t4-k4.v1.schema.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(schema(), indent=2, ensure_ascii=True) + "\n", encoding="utf-8")

    target.with_name("cuexis.gameplay-inline.t4-k4.v1.schema.json").write_text(json.dumps(inline_schema(), indent=2) + "\n", encoding="utf-8")
