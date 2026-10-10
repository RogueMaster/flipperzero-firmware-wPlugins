import itertools
import os

import SCons
from fbt.util import GLOB_FILE_EXCLUSION
from SCons.Node.FS import has_glob_magic
from SCons.Script import Flatten


_DIR_CACHE = {}


def _dirs_postorder(node, exclude):
    """Cache directory walks per root/exclusions for this SCons invocation."""
    key = (node, tuple(sorted(map(str, exclude))))
    if (cached := _DIR_CACHE.get(key)) is not None:
        return cached
    dirs = []
    for child in node.glob("*", source=True, exclude=exclude):
        if isinstance(child, SCons.Node.FS.Dir):
            dirs.extend(_dirs_postorder(child, exclude))
    dirs.append(node)
    _DIR_CACHE[key] = dirs
    return dirs


def _cached_glob_recursive(pattern, node, exclude):
    results = []
    for directory in _dirs_postorder(node, exclude):
        results.extend(directory.glob(pattern, source=True, exclude=exclude))
    return results


def _env_flag(name):
    return os.environ.get(name, "").lower() in ("1", "true", "yes", "on")


def _legacy_glob_recursive(env, pattern, node, exclude):
    """Original recursive-glob behavior, kept for fallback and validation."""
    results = []
    for f in node.glob("*", source=True, exclude=exclude):
        if isinstance(f, SCons.Node.FS.Dir):
            results += _legacy_glob_recursive(env, pattern, f, exclude)
    results += node.glob(
        pattern,
        source=True,
        exclude=exclude,
    )
    return results


def _literal_parent_glob(pattern, node, exclude):
    """Fast-path patterns whose directory portion contains no glob magic.

    Examples:
        core/*.c
        scenes/write/*.c
        helpers/protocol_support/mf_classic/*.c

    FAP source masks are relative to the app source root.  The old recursive
    implementation walked the whole app tree for every one of these masks,
    even though the literal parent directory already identifies where to look.
    """
    normalized_pattern = pattern.replace("\\", "/")
    if "/" not in normalized_pattern:
        return None

    parent_pattern, leaf_pattern = normalized_pattern.rsplit("/", 1)
    if not parent_pattern or has_glob_magic(parent_pattern):
        return None

    # Do not reject a literal parent merely because one of its path components
    # appears in the recursion-exclusion list.  Legacy GlobRecursive() uses
    # exclusions to prune recursive descent, but it still evaluates the explicit
    # root-relative pattern itself.  Apps such as Arduventure rely on this: FBT
    # passes "!lib" to avoid discovering lib/ through broad recursive masks,
    # while the manifest explicitly includes "lib/scr/*.cpp".

    # Keep the exact SCons node-mapping semantics used by the legacy
    # implementation: the match is evaluated from the original app/root node
    # with source=True.  Only the expensive recursive descent is skipped.
    # This matters for VariantDir-backed FAPs, especially C++ runtimes whose
    # entry-point object must remain a source-mapped node.
    return node.glob(
        normalized_pattern,
        source=True,
        exclude=exclude,
    )


def _node_paths(nodes):
    return [node.path for node in nodes]


def GlobRecursive(env, pattern, node=".", exclude=[]):
    exclude = list(set(Flatten(exclude) + GLOB_FILE_EXCLUSION))
    # print(f"Starting glob for {pattern} from {node} (exclude: {exclude})")
    results = []
    if isinstance(node, str):
        node = env.Dir(node)

    # Only initiate actual recursion if special symbols can be found in 'pattern'.
    if has_glob_magic(pattern):
        if _env_flag("FBT_GLOB_LEGACY"):
            results = _legacy_glob_recursive(env, pattern, node, exclude)
        else:
            results = _literal_parent_glob(pattern, node, exclude)
            if results is None:
                # Root-level masks reuse their directory walk. Matching and
                # result order remain the same as the original recursive glob.
                results = _cached_glob_recursive(pattern, node, exclude)
            if _env_flag("FBT_GLOB_VALIDATE"):
                legacy_results = _legacy_glob_recursive(env, pattern, node, exclude)
                if _node_paths(results) != _node_paths(legacy_results):
                    raise RuntimeError(
                        "Optimized GlobRecursive changed the source set/order for "
                        f"pattern {pattern!r} from {node}:\n"
                        f"optimized={_node_paths(results)}\n"
                        f"legacy={_node_paths(legacy_results)}"
                    )
    # Otherwise, just assume that file at path exists.
    else:
        results.append(node.File(pattern))

    ## Debug
    # print(f"Glob result for {pattern} from {node}: {results}")
    return results


def GatherSources(env, sources_list, node="."):
    """Glob every include pattern in sources_list, minus its "!exclusions".

    Both halves have to arrive in the same call - an exclusion only filters the patterns it is
    handed with. An exclusion is a bare leaf name, matched at any depth: "!plugins" drops every
    directory called plugins under node, not just the top one.
    Order is preserved, not sorted: it becomes the link order of whatever is built from it, so
    de-duplicating through a set() here would make the output depend on PYTHONHASHSEED.
    """
    sources_list = list(dict.fromkeys(Flatten(sources_list)))
    include_sources = list(filter(lambda x: not x.startswith("!"), sources_list))
    exclude_sources = list(x[1:] for x in sources_list if x.startswith("!"))
    gathered_sources = list(
        itertools.chain.from_iterable(
            env.GlobRecursive(
                source_type,
                node,
                exclude=exclude_sources,
            )
            for source_type in include_sources
        )
    )
    ## Debug
    # print(
    #     f"Gathered sources for {sources_list} from {node}: {list(f.path for f in gathered_sources)}"
    # )
    return gathered_sources


def generate(env):
    env.AddMethod(GlobRecursive)
    env.AddMethod(GatherSources)


def exists(env):
    return True
