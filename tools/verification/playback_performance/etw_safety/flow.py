"""Pure cleanup ordering. Effects are supplied by the Windows adapter or fault tests."""


def take_over(effects, deadline):
    """The guardian alone takes ownership after the exact controller is signaled dead.

    A false/unknown prerequisite must never fall through to a mutating ETW call.
    The adapter writes evidence before and after each effect, including exceptions.
    """
    effects.stop_controller(deadline)
    if effects.controller_alive():
        raise RuntimeError('controller death not proved')
    effects.drain_job(deadline)
    if effects.active_processes() != 0:
        raise RuntimeError('worker Job is not empty')
    return effects.stop_owned_and_prove(deadline)


def released_clean(effects, deadline):
    if effects.active_processes() != 0:
        raise RuntimeError('release with live worker descendants')
    if not effects.prove_absent(deadline):
        raise RuntimeError('release without exact ETW absence proof')
    return {'released_clean': True, 'active_descendants': 0, 'sessions_absent': True}
