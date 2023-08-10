# Linden LP-Based Postdrv ITDP

## Description

This feature aims to improve timing (wns/tns) by adjusting the positions of a small portion of cells (typically those on the critical path) at the stage of PlaceOPT after drv fix.
The movement is guided by the solution of an LP problem which takes timing into its formulation.

## Core Algorithm

Our core algorithm is almost the same as the following paper.
```
A. Chowdhary et al., "How accurately can we model timing in a placement engine?," Proceedings. 42nd Design Automation Conference, 2005., Anaheim, CA, USA, 2005, pp. 801-806, doi: 10.1145/1065579.1065792.
```
Differences are as follows:
1. Our timing model is more general (in terms of cell models) and we took care of more corner cases (e.g., unconnected pins).
2. We made a few changes in the constraints of the LP problem.
3. We shrinked the size of the LP problem by considering only a fraction of the original circuit.

## Usage

Use the following command to perform Linden LP-Based Postdrv ITDP:
```
linden_lpbased_postdrv_itdp -timing_data_file <pb_file>
```

Before linden_lpbased_postdrv_itdp, make sure you have the pb_file which contains the timing data. 
You can generate the timing_data pb file with the following instructions:
1. run rcx and pt signoff to get spef and sdf file
2. run command as follows:
    ```
    // read lef files
    // read lib files
    // read post-drv.def
    // read sdc file
    // source setRC.tcl file
    
    // spef and sdf are nice-to-have to get accurate RC and timing
    read_spef <spef_file>
    read_sdf <sdf_file>

    rsz::write_tdp_data <pb_file>
    ```

After linden_lpbased_postdrv_itdp, legalization should be followed.