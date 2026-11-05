# R4 preparation tools

These tools prepare O1 budgets and validate drafts. They do not accept budgets, register windows,
launch formal workloads, or change the historical PD-5 collectors.

Run from the repository root:

~~~powershell
python -B -m unittest discover -s tools/verification/execution_acceptance -p "test_*.py" -v
python -B tools/verification/execution_acceptance/audit_layout.py --build out/build/r4/pre-o1/msvc-on-debug --output out/evidence/r4/layout-new
python -B -m tools.verification.execution_acceptance.budgets --layout tools/verification/execution_acceptance/element-sizes-msvc-release.v1.json --output out/evidence/r4/budget-new.json
~~~

The layout probe compiles a sizeof-only executable with MSVC Release definitions. Its generated
headers come from the explicit configured build. Output directories must be new. The recorded
layout hashes project headers and the source of the frame/attempt accounting formulas; a different
ABI needs a new audit.

Budget coefficients are proposed design ceilings, not measured peaks or proven whole-heap bounds.
The proposal stays unaccepted and cannot authorize sampling. GPU allocations, CPU PCM,
fixture/observer memory, requested bytes, usable storage, and process commit/working set have
separate meanings.

The acceptance matrix runner now supports --build-root. For example, add
--build-root out/build/r4/preflight to keep each invocation in a fresh directory. --output alone
still selects only the evidence directory. Existing build trees and paths outside out/build are
rejected in isolated mode.

Scope, numbers, owner decisions and formal admission requirements are maintained in
[the R4 plan](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/plan.md),
[O1 proposal](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/caps-proposal.md) and
[registration draft](../../../docs/stage_plans/active/stage-07/05-rpa-new/r4/formal-registration.md).
