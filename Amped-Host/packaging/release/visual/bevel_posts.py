"""Round the four painted supports below the opening Miracle Mile rail."""

from hashlib import sha256

from refine_rail import area_vectors, edge_sets, np

IDENTITY = "be045b402851b763ffd863936acc7ea1b2d97336028bcf637b80860c2c370be3"
BEVEL_FRACTION = 0.18


def bevel(mesh):
    if mesh.identity != IDENTITY or mesh.slots != (0, 1, 2) or mesh.vertices.shape != (32, 3, 4):
        raise ValueError("Expected the four painted posts of the verified opening rail")
    posts = mesh.vertices.astype(np.float64).reshape(4, 4, 2, 3, 4)
    old_pos = posts[..., 0, :3]
    along = old_pos[:, :, 1] - old_pos[:, :, 0]
    if np.max(np.linalg.norm(along - along[:, :1], axis=-1)) > 0.001:
        raise ValueError("Expected parallel top and bottom rings")

    output = []
    faces = []
    for post in posts:
        for corner, pair in enumerate(post):
            before = pair + (post[(corner - 1) % 4] - pair) * BEVEL_FRACTION
            after = pair + (post[(corner + 1) % 4] - pair) * BEVEL_FRACTION
            # Preserve the flat faces and soften the corner with two facets.
            # Keep most of the original corner normal at the bevel's center.
            middle = 0.5 * pair + 0.25 * (before + after)
            for edge in (before, middle, after):
                normals = edge[:, 1, :3]
                normals /= np.linalg.norm(normals, axis=1, keepdims=True)
                output.extend(edge)
        offset = len(output) - 24
        for side in range(12):
            a, b = offset + 2 * side, offset + 2 * ((side + 1) % 12)
            faces.extend(((a, a + 1, b), (b, a + 1, b + 1)))

    vertices = np.asarray(output, dtype="<f4")
    indices = np.asarray(faces, dtype="<u4")
    verify(mesh, vertices, indices)
    return vertices, indices, {
        "source_draw_sha256": mesh.identity,
        "posts": 4,
        "cross_section_sides": [4, 12],
        "source_vertices": len(mesh.vertices),
        "source_triangles_excluding_strip_connectors": 32,
        "updated_vertices": len(vertices),
        "updated_triangles": len(indices),
        "bevel_fraction": BEVEL_FRACTION,
        "gpu_buffer_bytes": vertices.nbytes + indices.nbytes,
        "vertices_sha256": sha256(vertices.tobytes()).hexdigest(),
        "indices_sha256": sha256(indices.tobytes()).hexdigest(),
    }


def verify(mesh, vertices, faces):
    if (faces.max() >= len(vertices) or len(vertices) != 96 or len(faces) != 96
            or not np.isfinite(vertices).all()
            or not np.allclose(np.linalg.norm(vertices[:, 1, :3], axis=1), 1, atol=1e-6)):
        raise ValueError("Invalid support vertex or index data")
    before = mesh.vertices.reshape(4, 4, 2, 3, 4)
    after = vertices.reshape(4, 12, 2, 3, 4)
    for old, new in zip(before, after):
        positions = old[:, :, 0, :3].astype(np.float64)
        a = positions[1, 0] - positions[0, 0]
        b = positions[3, 0] - positions[0, 0]
        axis = (positions[:, 1] - positions[:, 0]).mean(axis=0)
        directions = np.stack((a, b, axis))
        directions /= np.linalg.norm(directions, axis=1, keepdims=True)
        old_local = (positions - positions[0, 0]) @ directions.T
        new_local = (new[..., 0, :3] - positions[0, 0]) @ directions.T
        # The measured cross sections are rectangular to within capture rounding.
        if not np.allclose(old_local.min(axis=(0, 1)), new_local.min(axis=(0, 1)), atol=0.001) or not np.allclose(
                old_local.max(axis=(0, 1)), new_local.max(axis=(0, 1)), atol=0.001):
            raise ValueError("Bevel changed the mounting height or original footprint")
        if not np.allclose(positions.mean(axis=0), new[..., 0, :3].astype(np.float64).mean(axis=0), atol=0.0002, rtol=0):
            raise ValueError("Bevel moved the support centerline")
        uv, new_uv = old[..., 2, :2], new[..., 2, :2]
        if (new_uv.min() < uv.min() - 1e-6 or new_uv.max() > uv.max() + 1e-6):
            raise ValueError("Bevel introduced UVs outside the original spans")
    positions = vertices[:, 0, :3].astype(np.float64)
    area = area_vectors(positions, faces)
    if np.min(np.linalg.norm(area, axis=1)) < 1e-6:
        raise ValueError("Bevel produced a degenerate face")
    # All four supports keep the original winding and intentionally open ends.
    old_area = area_vectors(mesh.positions().astype(np.float64), mesh.indices)
    original = old_area[np.linalg.norm(old_area, axis=1) > 1e-6].reshape(4, 4, 2, 3)
    flat_faces = area.reshape(4, 12, 2, 3)[:, 2::3]
    if np.any((original * flat_faces).sum(axis=-1) <= 0):
        raise ValueError("Bevel reversed the original side winding")
    boundaries = [edge for edge, use in edge_sets(faces).items() if use == 1]
    if (len(boundaries) != 4 * 2 * 12 or any(a % 2 != b % 2 for a, b in boundaries)
            or max(edge_sets(faces).values()) != 2):
        raise ValueError("Bevel left a seam or a non-manifold edge along a post")
