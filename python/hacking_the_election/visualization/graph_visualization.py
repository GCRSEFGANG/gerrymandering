"""Draws graphs (data structure)

Usage:
python3 graph_visualization.py <pickled_graph> (output_path | "None")
"""

import pickle
import sys

from PIL import Image, ImageDraw
from pygraph.classes.graph import graph

from hacking_the_election.utils.precinct import Precinct
from hacking_the_election.utils.visualization import modify_coords


def visualize_graph(graph, output_path, coords, colors=None, sizes=None, show=False):
    """Creates an image of a graph and saves it to a file.

    :param graph: The graph you want to visualize.
    :type graph: `pygraph.classes.graph.graph`

    :param output_path: Path to where image should be saved. None if you don't want it to be saved.
    :type output_path: str or NoneType

    :param coords: A function that outputs coordinates of a node.
    :type coords: function that outputs list of 2 float

    :param colors: A function that outputs an rgb code for each node, defaults to None
    :type colors: (function that outputs tuple of 3 float) or NoneType

    :param sizes: A function that outputs the radius for each node.
    :type sizes: (function that outputs float) or NoneType

    :param show: whether or not to show the image once generated.
    :type show: bool
    """
    graph_image = Image.new("RGB", (1000, 1000), "white")
    draw = ImageDraw.Draw(graph_image, "RGB")

    graph_nodes = graph.nodes()
    graph_edges = graph.edges()

    modified_coords = modify_coords(
        [coords(node) for node in graph_nodes], [1000, 1000]
    )
    
    if colors is not None:
        node_colors = [colors(node) for node in graph_nodes]
    else:
        node_colors = [(235, 64, 52) for _ in graph_nodes]

    if sizes is not None:
        node_sizes = [sizes(node) for node in graph_nodes]
    else:
        node_sizes = [1 for _ in graph_nodes]

    for edge in graph_edges:

        indices = [graph_nodes.index(v) for v in edge]
        centers = [tuple(modified_coords[i]) for i in indices]

        draw.line(
            centers,
            fill=(0, 0, 0),
            width=1
    )

    for center, node, color, r in zip(modified_coords, graph_nodes,
                                      node_colors, node_sizes):
        draw.ellipse(
            [(center[0] - r, center[1] - r),
             (center[0] + r, center[1] + r)],
            fill=color
        )

    if output_path is not None:
        graph_image.save(output_path)
    if show:
        graph_image.show()


if __name__ == "__main__":

    # If called from the command line, it is
    # assumed that the node attributes are precincts.

    with open(sys.argv[1], "rb") as f:
        graph = pickle.load(f)

    visualize_graph(graph, eval(sys.argv[2]), lambda node: graph.node_attributes(node)[0].centroid, show=True)