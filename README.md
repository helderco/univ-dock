# Dock - Loading/Unloading

This program manages a dock with some anchorages to unload and load cargo from and to many boats. It was made for an Operating Systems class, to practice the use of threads, semaphores and mutex locks in C (for Linux).

## Explanation

We have a docking bay with a limited number of docking spaces (N), including 3 anchorages, each with a spot at the crane for loading and unloading cargo, and 2 more waiting spaces. There should be more boats than available docking spaces at the bay (B > N), so that we can simulate the dock being full.

Each boat attempts to enter the docking bay, and dock at the first available anchorage. If no anchorage is available, the boat will dock at any of the remaining waiting spots. So, if we have 15 docking spots, and 3 anchorages each with 1 crane and 2 waiting spaces, that leaves us with 6 spaces where boats will wait for their turn to enter an anchorage space.

Forgive my terminolagy, I'm not very knowlageable in this subject. I had to adapt my terms midway into the project as I was understanding the issue. What I mean by an "anchorage", is a an area where a crane moves to load and unload cargo. So, if the anchorage has 3 spaces, the crane is moving cargo in one, and the remaining 2 are waiting for their turn. Each anchorage area also has a storage space, like a warehouse or hangar.

Each crane receives the first boat in their "anchorage area" and start to unload all cargo from the boat and load a random amount from that anchorage's  storage of cargo set for loading, which start off at a fixed amout when the program is run (taking care not to exceed the boat's maximum cargo capacity).

The dock closes when there is no more cargo in the anchorages to load to boats, nor boats with cargo to unload.

## Compile

    gcc -lpthread -o dock dock.c

## Report

The report was written in portuguese, using LaTeX. There should be available a pdf version in the [downloads page](https://github.com/helderco/dh-auto-backup/downloads), though.
