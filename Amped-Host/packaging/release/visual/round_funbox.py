"""Smooth the curved metal lip of the silver Miracle Mile funbox."""

from hashlib import sha256

from refine_rail import area_vectors, edge_sets, np, split_faces


def boundary(points, faces):
    unique, welded = np.unique(points, axis=0, return_inverse=True)
    incidence = edge_sets(welded[faces])
    if max(incidence.values()) != 2:
        raise ValueError("The trim should have manifold sides")
    seams = {tuple(sorted((tuple(unique[a]), tuple(unique[b]))))
             for (a, b), uses in incidence.items() if uses == 1}
    return seams, len(unique) - len(incidence) + len(faces)


def round_lip(mesh):
    if mesh.vertices.shape != (220, 3, 4) or mesh.material != 4:
        raise ValueError("Expected the measured long funbox trim")
    original = mesh.vertices.astype(np.float64)
    points = original[:, 0, :3]
    lo, hi = points.min(axis=0), points.max(axis=0)
    upper = points[points[:, 1] == hi[1]]
    corners = np.array(np.meshgrid([upper[:, 0].min(), upper[:, 0].max()],
                                  [(lo[1] + hi[1]) / 2],
                                  [upper[:, 2].min(), upper[:, 2].max()])).reshape(3, -1).T
    ownership = np.argmin(np.linalg.norm(points[:, None, :] - corners, axis=-1), axis=1)
    # This variant stretches the molded profile 2× in X and Y.
    scale = np.array([2.0, 2.0, 1.0])
    area = area_vectors(points, mesh.indices)
    faces = mesh.indices[np.linalg.norm(area, axis=1) > 1e-6]
    old_boundary, topology = boundary(points, faces)
    if len(old_boundary) != 8 or len(faces) != 200 or len(np.unique(upper, axis=0)) != 4:
        raise ValueError("Unexpected funbox deck interface")
    output = list(original)
    middles = {}
    deviations = []
    for a, b in edge_sets(faces):
        if ownership[a] != ownership[b] or abs(points[a, 1] - points[b, 1]) < 0.002:
            continue
        center = corners[ownership[a]]
        radial = (points[[a, b]] - center) / scale
        radius = np.linalg.norm(radial, axis=1)
        cosine = np.dot(radial[0], radial[1]) / np.prod(radius)
        if cosine < 0.6 or cosine > 0.999:
            continue
        direction = radial.mean(axis=0)
        direction /= np.linalg.norm(direction)
        midpoint = original[[a, b]].mean(axis=0)
        midpoint[0, :3] = np.clip(center + direction * radius.mean() * scale, lo, hi)
        normal = midpoint[1, :3]
        radial_normal = direction / scale
        radial_normal /= np.linalg.norm(radial_normal)
        normal[:] = 0.7 * normal + 0.3 * radial_normal
        normal /= np.linalg.norm(normal)
        middles[(a, b)] = len(output)
        output.append(midpoint)
        deviations.append(np.linalg.norm(midpoint[0, :3] - (points[a] + points[b]) / 2))
    triangles, parents = split_faces(faces, middles)
    vertices = np.asarray(output, dtype="<f4")
    verify(mesh, vertices, triangles, faces, parents, old_boundary, topology)
    max_change = float(max(deviations))
    if len(middles) < 24 or not 0.002 < max_change < 0.025:
        raise ValueError("The lip did not retain the expected bevel radius")
    return vertices, triangles, {
        "source_draw_sha256": mesh.identity,
        "source_vertices": len(mesh.vertices),
        "source_triangles_excluding_strip_connectors": len(faces),
        "updated_vertices": len(vertices),
        "updated_triangles": len(triangles),
        "split_edges": len(middles),
        "exact_original_attributes_and_deck_seams": True,
        "maximum_deviation_from_original_chord": max_change,
        "gpu_buffer_bytes": vertices.nbytes + triangles.nbytes,
        "vertices_sha256": sha256(vertices.tobytes()).hexdigest(),
        "indices_sha256": sha256(triangles.tobytes()).hexdigest(),
    }


def verify(mesh, vertices, triangles, faces, parents, old_boundary, topology):
    if (vertices[:len(mesh.vertices)].tobytes() != mesh.vertices.tobytes()
            or triangles.max() >= len(vertices) or not np.isfinite(vertices).all()):
        raise ValueError("Trim changed an original vertex or produced invalid data")
    points = vertices[:, 0, :3].astype(np.float64)
    new_area = area_vectors(points, triangles)
    old_area = area_vectors(points, faces)
    if (np.linalg.norm(new_area, axis=1).min() <= 1e-6
            or np.any((old_area[parents] * new_area).sum(axis=1) <= 0)):
        raise ValueError("Trim changed winding or introduced a degenerate face")
    new_boundary, new_topology = boundary(points, triangles)
    if new_boundary != old_boundary or new_topology != topology:
        raise ValueError("Trim opened a crack or changed the original deck interface")
    for extrema in (np.min, np.max):
        if not np.array_equal(extrema(mesh.positions(), axis=0), extrema(vertices[:, 0, :3], axis=0)):
            raise ValueError("Trim changed the overall footprint")
    n = len(mesh.vertices)
    if not np.allclose(np.linalg.norm(vertices[n:, 1, :3], axis=1), 1, atol=1e-6):
        raise ValueError("Trim produced a non-unit normal")
