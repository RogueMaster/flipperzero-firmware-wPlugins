import trimesh

cube = [
    [3, 3, 3],
    [2, 3, 3],
    [2, 2, 3],
    [3, 2, 3],
    [3, 3, 2],
    [2, 3, 2],
    [2, 2, 2],
    [3, 2, 2],
]


def voxel_mesh(points, size=1.0):
    """
    Create a voxel mesh from a list of 3D coordinates.

    Parameters:
        points : list of (x, y, z)
        size   : float, size of each cube

    Returns:
        trimesh.Trimesh : combined voxel mesh
    """
    cubes = []
    for p in points:
        cube = trimesh.creation.box(extents=(size, size, size))
        # move cube so min corner is at p
        cube.apply_translation((p[0] + size / 2, p[1] + size / 2, p[2] + size / 2))
        cubes.append(cube)

    # merge all cubes into a single mesh
    return trimesh.util.concatenate(cubes)


points = [
    [7, 4],
    [8, 4],
    [3, 5],
    [4, 5],
    [6, 5],
    [9, 5],
    [3, 6],
    [5, 6],
    [10, 6],
    [3, 7],
    [5, 7],
    [10, 7],
    [3, 8],
    [4, 8],
    [6, 8],
    [9, 8],
    [7, 9],
    [8, 9],
]
# points = [[3,2],[4,2],[5,2],[6,2],[7,2],[8,2],[2,3],[9,3],[2,4],[9,4],[3,5],[4,5],[5,5],[6,5],[7,5],[8,5],[3,6],[6,6],[8,6],[4,7],[6,7],[9,7],[2,8],[4,8],[7,8],[3,9]]
for i, j in points:
    if i == 3:
        print(i, j)

new_points = [
    [4, 6],
    [4, 7],
    [6, 6],
    [6, 7],
    [7, 5],
    [7, 6],
    [7, 7],
    [7, 8],
    [8, 5],
    [8, 6],
    [8, 7],
    [8, 8],
    [9, 6],
    [9, 7],
]
# new_points = [[3,3],[3,4],[4,3],[4,4],[5,3],[5,4],[6,3],[6,4],[7,3],[7,4],[8,3],[8,4],[2,9],[4,9],[4,6],[6,8],[9,6]]
for i in new_points:
    points.append(i)
coords = []
for i, j in enumerate(points):
    coords.append([j[0], j[1], 1])

mesh = voxel_mesh(coords, size=1.0)
mesh.show()
mesh.export("fish.stl")
