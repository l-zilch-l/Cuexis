# T-01 基线清单

状态：evidence

更新日期：2026-11-23

<!-- cuexis-meta
id: EVIDENCE-stage-07-rr.t-01-baseline-manifest
status: evidence
authority: none
date: 2026-11-23
-->

- 基线分支：`stage-7-rr`
- 基线提交（HEAD）：`a22e255f6716b22f96ad8a183136eae4fb04ac2f`
- 基线 tree：`7869115460b0c17a1f833f30700cd06f34e8e3c3`
- BASE_SHA（采用绑定）：`a22e255f6716b22f96ad8a183136eae4fb04ac2f`
- 工作树状态：干净

哈希规则：规范哈希取基线提交中该路径的 blob 字节（`git cat-file blob <BASE>:<path>`），可在任意 checkout 复现。
`manifest.json` 另记 `sha256_worktree`（本工作树字节）；仅在 Windows `core.autocrlf=true` 时按行尾不同，内容归一化后相同。

## 工作树与未跟踪清点

```text
(empty: no modified, staged or deleted tracked path)
```

`git diff --name-status` 与 `git diff --binary` 均对 BASE_SHA 运行，输出为空。
`git ls-files --others --exclude-standard` 计数：0

```text
(empty: no untracked file in the whole repository)
```

工作树行尾变体（规范哈希与工作树哈希不同，内容相同）：134 个；内容差异：0 个。

## 派单全局输入哈希

| 路径 | 字节 | SHA-256（blob@基线） |
| --- | --- | --- |
| `AGENTS.md` | 6209 | `f594e1003cab0a21eab9b0c1081040927cef285b895d74c11564a8b023acd078` |
| `CMakeLists.txt` | 58820 | `da88b7c1c6c974f55121203f332a269dbec2b81ebbe00ac76dec104149cb3c84` |
| `CMakePresets.json` | 14023 | `5096b0edc3f3277cc76aaae4750f8310c52d5532dee671ab85d8173df2219d08` |
| `vcpkg.json` | 1421 | `c71a3dbe45a06ebb89e70b58f0a22047af4d38a47b364dd05ec9692a0c9f82f8` |
| `cmake/CuexisVersion.cmake` | 3530 | `dcfd41d0a82729a111e76db53dc7a63dc562259ba07d85fead733b128c3f2982` |
| `cmake/VerifySharedExports.cmake` | 7397 | `2e3310391d4ff42d0cfb4da6d2a9df26114c6daa8e223c151b88d4af6fb5e107` |
| `cmake/VerifyExternalConsumer.cmake` | 27353 | `e681799d2b70668867622bfb3666b444a9ad22a11ad6969cccdb3f2664ce5f80` |
| `cmake/VerifyGameplayInstalledConsumer.cmake` | 4643 | `3b4add0b7ec5e8795eca7ac0cd0e7271595584ca8f3890a3ec87015563312957` |
| `docs/HANDOFF.md` | 4107 | `f7c4bfcf2da54206134664d56f32c3f6a15dc8318bfa979236081953ca450bbe` |
| `docs/CURRENT_STATUS.md` | 5608 | `86c48f1917f83e394f88f108fa213685d060701efb86cc95b8624cbf3a213e9d` |
| `docs/MAP.md` | 34796 | `cadc65fedacfd4e85cd0cc99bd6ac431db79d74bea9af3a6fe6840cdbc89cef9` |
| `docs/PROJECT_GUIDE.md` | 10826 | `0580168bbc43971da5c5bf50d9cde47dbf65035a3c5e5409f67154c1ee60c213` |
| `docs/ROADMAP.md` | 15956 | `6a21e3e806025920863bee7644403ff7224d9b8cfd6bc61c3acd68cfc551eba1` |
| `docs/DOCUMENTATION_POLICY.md` | 15403 | `61122b7c112c6d22428b8630fa734526ca4b3f08b6b64b5dd0455336370605a5` |
| `docs/guides/BUILDING.md` | 35983 | `d525e68c293fc43256b433985c586ce31b4900901e2b5adeaa50ac198968add9` |
| `docs/guides/CODE_POLICY.md` | 6901 | `401282e0724723dc666be8e92af1811442b51b266151569bdd734d747af69393` |
| `docs/guides/DEPENDENCY_POLICY.md` | 10517 | `1c8d03f05ce2ac52c1d3b53577c1f4f5e948dd2ae94c19d2b3ec4bcadbf64ccb` |
| `docs/guides/VERSIONING.md` | 29499 | `5e6d4da57ffe7c16f426b1dfb2a07f9022bc59d0d44b723fa58e78d49ec2a404` |
| `docs/guides/LIMITS.md` | 3304 | `a63f20717b46e391651235cc9809c4dd24a6842b2c4084193d659aae101d043c` |
| `docs/guides/DIAGNOSTIC_CODES.md` | 7762 | `907f730ab9457b297e4e083bf7065c6e0b20588d5ef1f52b0b95ebb7203e11e0` |
| `docs/guides/COMMIT_AND_PR_CONVENTION.md` | 2130 | `a339d94b7006352b6a25f9760fe67c1d522fdf3e55c19d9ea9f2f4571db03e44` |
| `docs/formats/README.md` | 9075 | `db8c8f91e5ee844a32c743b74bb277210efd00b7b08b4d45c6a715aaff99923f` |
| `docs/formats/CHART_FORMAT.md` | 29135 | `58763902f688768fc03b35eb11d391d7fef642594a38e5bd2683b7520dd7f894` |
| `docs/formats/CHART_V4_FORMAT.md` | 24309 | `7ff5bfe346bf87ef3300d3c5d4f2d83e4f42a5756f29a3be4dab95d7f14d3ca6` |
| `docs/formats/CXC_FORMAT.md` | 20260 | `222250d55308fdb58758b50502dd2340eaf79d1356e424640672104497a1dc1b` |
| `docs/formats/CXT_FORMAT.md` | 8573 | `8ec8c09de6987c9b1cda3c8891c357a1727ccfc120ff01f4f03cf471aaa3b8f0` |
| `docs/formats/CXT_V2_FORMAT.md` | 32650 | `19bb96bada690b470552be23b58ad3ca004d420b6261fb7a89774d2936790977` |
| `docs/formats/GAMEPLAY_V2_SPEC.md` | 280547 | `ea4b0c018dc850e390ed4b671efb183c689609768222c3f8f536ae3d80d2f1ce` |
| `docs/formats/GAMEPLAY_CAPSULE_V2_FORMAT.md` | 37017 | `b969d6a22ecc617305b0e04d6057c96ea4a173193f3c929866c6de9f599d4cb6` |
| `docs/formats/GAMEPLAY_GRAPH_V1_FORMAT.md` | 11460 | `91728c091b013df45eba7c3e5401943bb56d847a4f98a7fc537bb4e9b405f1e6` |
| `docs/formats/PACKED_CHART_FORMAT.md` | 46229 | `1feb9a014163e6c0304020b1dd5bb59f037474fe929bb1100af510a16acf6c03` |
| `docs/formats/TIMING_MODEL.md` | 7647 | `c876e208b77ef872cb9563ef3afc26185be33ed8d9b436efd6e52c6d81bb9668` |
| `docs/formats/ANIMATION_MIXING.md` | 8205 | `0b54dd811ae6a0c44ca3fbe606b80683c5a546a00806147c8b5e382d339ae8f9` |
| `docs/formats/PORTABLE_PRESENTATION.md` | 33244 | `56c84fda0b6f37dcffd6a59065bfd4e8231e0d048f84b898891d5192b3e7858e` |
| `docs/formats/execution-recovery-wire-v3.md` | 5509 | `a26b40f4d8615e74fd35bfa4d7745c47c05a9d9326ea69bd518bca5a39df0c28` |
| `docs/formats/rpa-execution-contract.md` | 32568 | `736016609d6b6400ef71c3512c675ff66b6af7e1b7b3ccb355cc680c3d5af0b7` |
| `docs/api/README.md` | 3287 | `6441cf12754af87a9112de122704530290ce0f04a6a8832cc9db619a4575dd76` |
| `docs/api/GAMEPLAY_V2_ABI.md` | 182149 | `c90799acd1b21f3de6fd52ec4820881311cee576f15318b4f6751da53c191948` |
| `docs/api/playback-session.md` | 23353 | `980ac2b02278a5e2f2281b6764312ec0664f053dd7580a77da33b4cdb4ab5f90` |
| `docs/api/realtime-host-boundary.md` | 35173 | `e60ebf558c46183d90b73308423f178c4a19484b20a5f85186514675d53952e2` |
| `docs/api/frames-digests-and-timelines.md` | 5316 | `7bc7ab7da0c3fbb70a9262598f4ee19c2d08b83b77e19c257a6bfd69064d0f7b` |
| `docs/api/presentation-and-capabilities.md` | 3579 | `a3ee339f3e4278488159880769261e4011031f630036ac0045f51a3f7021ca2c` |
| `docs/api/sources-and-content.md` | 3462 | `bb5a3b1259135a4d0099f7be569dec7dab7a9ed8977bedb49a87662c1049edac` |
| `docs/api/execution-coordination.md` | 7030 | `06182ceaea3d58b55bd28b4a71e1be9a09f10129a4ea9bf98218c7c48f19aa53` |
| `docs/api/gameplay-v2-execution-types.md` | 16871 | `390a43e318bea5d3cd58900ba36a461e62951ed7e777dc5d379c07308a627eea` |
| `docs/api/internal-module-catalog.md` | 3099 | `5d432bf9ed8e7339750e90a880064e2202296688c6d3696a8d2a74d002370098` |
| `docs/api/diagnostics-identity-and-compatibility.md` | 3543 | `eb2ffad761db107576012b1f7cf72e3bbd6fbdfdb8ab9d782e51dc7ef33ca605` |
| `docs/architecture/OVERVIEW.md` | 5048 | `65d3e6e4aea31debcac68d99b564b685e2a2f4d77a58b1f3a4ba1790116fd0a7` |
| `docs/architecture/MODULE_BOUNDARIES.md` | 6366 | `6cd8d78ffe6fea7d4bdd0d764409451151cc8dbd71f4615f05680fc523989561` |
| `docs/architecture/RUNTIME_SESSION.md` | 14611 | `773f0e481b86492ec9b49dad06e6b9218199486b9ef2777a53b93ccbfa109a29` |
| `docs/architecture/CHART_REPRESENTATIONS.md` | 7893 | `bbb3fca7c3b0ab8dadfa84416408217794292bed3df762a9f302ee0d7151111e` |
| `docs/architecture/GAMEPLAY_ABSTRACTION_MODEL.md` | 12315 | `348d85b0865e7d1523dcabd637c02efd27054ab568699b29376899fd2b2b4f35` |
| `docs/architecture/execution-state-and-recovery.md` | 15071 | `8d0ac10ced0dad022a34eb6a5f0be2d4d6f3e7cecbb6f0abd5e9ea35b9f0ad5e` |
| `docs/architecture/PLAYER_APPLICATION.md` | 23686 | `a62c1d821a2e31cc7d1b8c47c9253f8407b9f89c6bb382a9e319b65dd98b01c6` |
| `docs/adr/0041-legacy-format-exit-policy.md` | 3245 | `389f3053dfab5bc0ee115654b82ee484dc408a33f8bae63d21cee1e8b359f316` |
| `docs/adr/0046-realtime-playback-coordination.md` | 6857 | `cf31e5f7a8934eef9e815a11f4209f99d12d0fd771cc2cabe37ca300f972dd9d` |
| `docs/adr/0047-pd-feedback-lifetime-and-delivery-retirement.md` | 4271 | `84cd4909351946a4a032c075fc86f025146c2ab990c3ce7bd21e1d655ceac36a` |
| `docs/adr/0030-playback-preview-api-version-and-result.md` | 4982 | `305cfe1bbebcd2045927c4e6be83fde2aaafcea56acb2b6cd8fbb641e28654bf` |
| `docs/adr/0033-cpp-shared-library-preview-boundary.md` | 12853 | `7bb9e9e4441842277e399f50f57753979fb4296c2815851057b69ae4259eca0b` |
| `docs/templates/report.md` | 795 | `f890650bb1e850f3775868b5f4f5cb450c708d5b7eec0505c6566eeacbeea7e1` |
| `docs/templates/task.md` | 1780 | `5aff41ee0680700c22ba4216fe1803bdae741268721847f179e53bde962435c7` |
| `docs/templates/plan.md` | 5920 | `95287d75a1af9f7e03c570002e606b8c49f7f675b6e9648b165f7083f2970c74` |
| `docs/guides/CHART_V5_AUTHORING.md` | 16803 | `2e2acf7a0760e8ae7ed18e6d6cedfcfe0a17e0a5f2727a20c8906e763b33cfba` |
| `docs/stage_plans/active/chart-format-update-for-v5/plan.md` | 66372 | `3548a62ac3773a8620f089b0894b6c43427189a3d5349af02148dd9394095915` |
| `docs/stage_plans/active/stage-07-rr/README.md` | 2630 | `a36f56a8637285a96b149cc15eb0d7a4dc3daae0303c64bcba2f4d66674e1d77` |
| `docs/stage_plans/active/stage-07-rr/plan.md` | 58607 | `84cb51fb8cc8c7dbe401739a2e479ea75cd14ea8e551902addd1345027fe789d` |
| `docs/stage_plans/active/stage-07-rr/schedule.md` | 856 | `b1b032142a49b794b9c4492464063f6d323b2b701949381d6150723db163a2ea` |
| `docs/stage_plans/active/stage-07-rr/acceptance.md` | 11635 | `9c0d9536305aeee93a4e530b43fd0a8170bb4b390a333795086de2bea342557a` |
| `docs/stage_plans/active/stage-07-rr/findings.md` | 168100 | `eae5d9b174123e151928335078d8ec5dbcace06c55a8aec7568747331e417f3c` |
| `docs/stage_plans/active/stage-07-rr/design-readiness.md` | 11734 | `2fb24dfd06fd29a002ce9adb53d3f8d28594b4edbc167d40afff76a1fb7caf94` |
| `docs/stage_plans/active/stage-07-rr/architecture-design.md` | 87557 | `69a77a8951ea128c6a28f98b70331fb2871f1ecd5f310684d8ddeef2ad8c89e7` |
| `docs/stage_plans/active/stage-07-rr/simplification-proposal.md` | 3242 | `c8aa971c9faf218f8f894c49681a10ce0982e7277f9bc97c62b092005cd4f237` |
| `docs/stage_plans/active/stage-07-rr/contracts/runtime-timing.md` | 127368 | `bdb72b3aba8d36fcb939d25005fa9c2a7a9a1f8c8043392d2525bc98523e23e2` |
| `docs/stage_plans/active/stage-07-rr/contracts/prepared-artifact.md` | 98359 | `e9c4f8fab69778fffd4b6d4ef33e0f054fba32dc62027f5c245e9c1eaa5cb8b2` |
| `docs/stage_plans/active/stage-07-rr/contracts/render-player-writer.md` | 135212 | `0939210386334eea8baec34a796f0a52312766c12e078b9b2c369aa911df3354` |
| `docs/stage_reports/stages/stage-07-rr/verification/2026-11-22-design-readiness-review.md` | 33828 | `18b297b6d7ac0cd477d0896ac68e45179c3a79d886861551e3c728f85c4f4f7f` |
| `docs/stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-executor-review.md` | 7371 | `ae669ed4311bc22ad2f873e352141bc464ea84105e6c7d21d0b2bc02bb20ad3c` |
| `docs/stage_reports/stages/stage-07-rr/verification/2026-11-22-astra-review-resolution.md` | 10268 | `31dca2c8ed2bda961e1a5cc6bb25b7eb8e349f3ddffdfb2b601c8046caa238e6` |
| `docs/stage_reports/stages/stage-07-rr/verification/2026-11-22-checkout-review-resolution.md` | 10961 | `66d82e4a6db6e786386e0f7098d424e01a2323e805b1629fc9e3b271b5999321` |
| `docs/stage_reports/stages/stage-07-rr/handoffs/2026-11-23-execution-readiness-handoff.md` | 9019 | `1bedd8512e3200d0e0569574b25d79dcc595f973d352690a17e80c4bccdad9cb` |
| `docs/stage_reports/stages/stage-07-rr/decisions/2026-11-23-design-decisions-and-schedule.md` | 5138 | `da66245279fae9bed03df9e95b1e5ba34c207cb824ada30643694362372fc8a7` |
| `docs/stage_reports/stages/stage-07-rr/implementation/2026-11-23-documentation-governance.md` | 7387 | `ca6b440f619aeaf6dfa38cbfdf7224dc37f0a4b76285735a986c8768d33c9d27` |
| `docs/stage_reports/stages/stage-07-rr/evidence/documentation-governance/manifest.json` | 5368 | `090b718c031aec4e32ab2a3c1bdf3cd6a4659f65a9a8fc782d4d68d35cbc1f15` |
| `docs/stage_reports/stages/stage-07-rr/evidence/documentation-governance/migration.json` | 60993 | `3c9aee4ace47477fc8d6ead89da3484f4a504f4eacc471742cc8f4177ca3eb7b` |
| `tools/verification/check_plan_schedule.py` | 19149 | `afbdc10a1e8596904276909fec5c68038b6acc5cc52e6be6a963b112ae49523d` |
| `tools/verification/test_check_plan_schedule.py` | 18092 | `8c651ea330cb3dab73e65a9bb746e53d21786e67212f45a8e99e089389a4e9cd` |
| `tools/verification/check_docs.py` | 46472 | `9997f901d8ad0edb499327ce8e9a92bae55ac4c0906c4c8f419eaf6e453f9836` |
| `tools/verification/check_docs_facts.py` | 11874 | `2dbbe551ad474580d7db0e1a18b50ca4d0f100aab90586dfd74e043b1079b6f5` |
| `tools/verification/check_findings.py` | 15352 | `bfa6a5dfdc76c1c5e96177bcb01c804e2d0a57fd1ab15a0aae0b6869fe612e06` |
| `tools/verification/check_docs_governance.py` | 17948 | `3c3beb3a1d07123042e21b6707934128f0b20601184aaa3909a7078331fddbf6` |
| `tools/verification/gen_docs_map.py` | 13816 | `44d2020f0a3f14df13f1281bc0333e68b88cbcf88afe63fd570204b5e4e74e80` |
| `docs/governance_rules.json` | 63972 | `732c154b749be5870fbbcc90327a5e49a6dfdfc137fb69fbb42ca83ca1082714` |
| `docs/status_contract.json` | 100 | `5367b3449e8d191bbf1fed209ec95aea9558fc19a02e4ef8bb72079a64439950` |
| `tests/fixtures/chart_format_update/golden/README.md` | 1723 | `023eab6f08fdf6e340077ab03258420ae306f9f03ca6daf40687fe653402f39e` |
| `tests/judgement/CMakeLists.txt` | 1574 | `664bc5ebba72a374117cb7f3a5a1e7fd7c9ffb76f0df80d5249c739207bc64bb` |
| `tests/chart/CMakeLists.txt` | 3383 | `4ad56af782bb77b57463099300dc289c92fd6e29ccf2357900a6e4bf89d9130a` |
| `tests/playback/CMakeLists.txt` | 6183 | `50c5aba69a2fca1e6e8b0e0de89a2dee9736a5421c4f51cc9e3c4bab2a9b645d` |
| `tests/rpa/CMakeLists.txt` | 2816 | `f89f578027728bf4117f09e0c82a0894168166ce0b39cba23b83cca8da1efa3e` |
| `tests/player/CMakeLists.txt` | 3246 | `2f3bcb89f5d0d94c3f6155f63ff32bed09758916ec85bb01f4ee941955473162` |
| `tests/cxc/CMakeLists.txt` | 1707 | `6ddd09fe19e179276fdad5aad3f4b3c3c9015c60017aa60770e7516a1cbf5a5a` |
| `tests/gameplay_packed/CMakeLists.txt` | 1133 | `9ceb7fed952837a93bb9e43cb4c1446de3946e26d0dd9a8e65dd232732c1ae1f` |
| `schemas/cuexis.animation-template.v1.schema.json` | 5343 | `826f0583400dd74ac6c4de173e7584620bb111ca4f615c1a1199df71d4518142` |
| `schemas/cuexis.animation-template.v2.schema.json` | 10723 | `4f2e2f2e6279e1a11919e8182d075425e6298427f100e0a229d5b08bda4014b7` |
| `schemas/cuexis.asset-index.v1.schema.json` | 1493 | `c7987cfbc11df32a899d4c036761765f12165432f8063cf758e7438a3d3bf5ea` |
| `schemas/cuexis.asset-index.v2.schema.json` | 1690 | `02dc3e80967a5dd7a25ddd7608edc4835b779c517d6b7cc3cb950d9948baa5ad` |
| `schemas/cuexis.asset-index.v3.schema.json` | 1864 | `67a672c1aea7990de37f18f64acce7c9ad3f07ee0fbdf5638c466cc289b47b81` |
| `schemas/cuexis.audio-device-profile.v1.schema.json` | 1751 | `5d8c4b527e7a2a2b7c7421bf7c6e70b26a15b2c9431152321b52cec2ffa9741c` |
| `schemas/cuexis.chart-entry.v1.schema.json` | 2906 | `521aa26f70597599713fd7689af49e1ca934fe6fd5680259b3dc2e77c634fd34` |
| `schemas/cuexis.chart.v1.schema.json` | 11968 | `78484f2342ed983f289ec791098a063cfbfcc4d6b639b3cddb85a43217bea466` |
| `schemas/cuexis.chart.v2.schema.json` | 12260 | `36ead69eb89f18711778a441de8f09126faff747059fb1b99fe502cf497245f9` |
| `schemas/cuexis.chart.v3.schema.json` | 15377 | `49ee13db60af70ac6ba13e34638da3aca3e61c2d68b828670c18797686b92ad1` |
| `schemas/cuexis.chart.v4.schema.json` | 26433 | `5543a486f0a41467b127de0ef4896d4e0266687b49e96860480ab02984132516` |
| `schemas/cuexis.cxc.v1.schema.json` | 1753 | `3db7cac34ec313950c548c008792536cb92a7d8c9228809049e70428bba5a0b2` |
| `schemas/cuexis.gameplay-capabilities.v1.json` | 1207 | `ae987432a030a8268ac4da46a6198a43449b201453156c47329e30deac2c4c64` |
| `schemas/cuexis.gameplay-diagnostics.v2.codes.json` | 104953 | `96d43a50830b0e6c082519d18d82627ccf8f714c33ed433f6d2c1b4db38d7fc0` |
| `schemas/cuexis.gameplay-graph.v1.schema.json` | 14342 | `9c58e51e81e50cd3d0148cf951f2f9eaac67bd7f75a8388ed23b61353a712974` |
| `schemas/cuexis.player-preferences.v1.schema.json` | 909 | `e522249871b7dc656c0678d73150439a21edb63bdc8c839c3d78e5c365a9aae7` |
| `schemas/cuexis.project.v1.schema.json` | 1976 | `8701e5c54973ea3529e4dbfc11e9c7107853ea63f5fa5e7fe6b17fcabe0f82ac` |
| `schemas/candidate/cuexis.gameplay-author.t4-k4.v1.schema.json` | 37555 | `90715406ef1dde3d9e801b07344d4cb79e25e9902fe60818bed3a2aa9572c0ca` |
| `schemas/candidate/cuexis.gameplay-inline.t4-k4.v1.schema.json` | 47423 | `aa934e69692be0123bdd9346be86019f18644e6c9d3d87defff6a8db15b9bdfa` |

## 逐卡采用绑定

| 任务 | scope code | 卡内范围 | 卡片 SHA-256 | 链接输入数 | 证据目录 |
| --- | --- | --- | --- | --- | --- |
| T-01 | `DOC` | 现行图 | `1524dfa9c6b6c30460762b8b09588348819e30f667a05d549b54e75e9c58bfaa` | 9 | `out/review/stage-07-rr/T-01/` |
| T-02 | `DOC` | 现行图 | `32d1684f7af55625d6dd1ef550147e06de9b2573041f48c32d69f049771471ed` | 9 | `out/review/stage-07-rr/T-02/` |
| T-03 | `RR2` | 现行图 | `6b185dd3594b454a6d3d119e60a40fddedf38dc6a799fb90bc4b197a6b3ab922` | 19 | `out/review/stage-07-rr/T-03/` |
| T-04 | `R1` | 现行图 | `d5015e61a5c27be3a6814d6cd143ae89c0ddfa4ef3a453365f5c016321518811` | 16 | `out/review/stage-07-rr/T-04/` |
| T-05 | `R5` | 现行图 | `f5b4ecdafc0eeca7d08de95cdc469e6f8328aa760894bc18c9b7325230499f8a` | 6 | `out/review/stage-07-rr/T-05/` |
| T-06 | `R5` | 现行图 | `a5b0e5e52906f1645b37f14cdb8aabbdf9ce1f652c504b21fc4ef3f557c4690c` | 13 | `out/review/stage-07-rr/T-06/` |
| T-07 | `DOC` | 现行图 | `6f05dd8f06b718bf14f0747bf45f93956fe68d4fd43c54311f646aebb8903531` | 17 | `out/review/stage-07-rr/T-07/` |
| T-08 | `RR4` | 现行图 | `55a93a299a6ae6fbe867212069f20b1dcb9da3af9b34b433c463ca6c4cc341aa` | 12 | `out/review/stage-07-rr/T-08/` |
| T-09 | `DOC` | 现行图 | `d04bf028a2cc62158689231512565b52348eee44508cda9bea29b7b3a5821004` | 10 | `out/review/stage-07-rr/T-09/` |
| T-10 | `NEXT` | NEXT（本轮实验实现） | `8cf98f0f085e9b43942c5a8e3bd8d453ade7778cf2f8775b06487aa7bc337a2a` | 12 | `out/review/stage-07-rr/T-10/` |
| T-11 | `NEXT` | NEXT（本轮实验实现） | `75a88a3e256d887900460e133ff9cb5d2e6548b414c2760e032832fcbd1c718f` | 7 | `out/review/stage-07-rr/T-11/` |
| T-12 | `NEXT` | NEXT（本轮实验实现） | `fef31577a6cb6bb3340dd2196bc16f4cb09fc8ba3ec147b32f805375025c60dc` | 9 | `out/review/stage-07-rr/T-12/` |
| T-13 | `NEXT` | NEXT（本轮实验实现） | `6a14e8cc3268b3acda0c5d8c7b4dcb70a92c90f79235c3f663c2f051c8ed8580` | 14 | `out/review/stage-07-rr/T-13/` |
| T-14 | `NEXT` | NEXT（本轮实验实现） | `070e5aa4c45b5748409ad5aa4b8f7937ee3c4a7ea7c8e342a7612dc65b7c6af8` | 8 | `out/review/stage-07-rr/T-14/` |
| T-15 | `NEXT` | NEXT（本轮实验实现） | `7fa6d7360fa0837467c260df33650c811547333309d283d02640466a469a91bf` | 9 | `out/review/stage-07-rr/T-15/` |
| T-16 | `NEXT` | NEXT（本轮实验实现） | `b334209b4f2d8bcbbe9b6d36cd735bc6e4db87912bca10c7101029b2525db010` | 8 | `out/review/stage-07-rr/T-16/` |
| T-17 | `FIX` | 现行图 | `074a122baf0de7e8c275d1c9c5af17242884cdbe77ef4046d3e92aa9b2eba0ce` | 18 | `out/review/stage-07-rr/T-17/` |
