for topo in "sw_50_routers" "sf_50_routers"
    # path data
    python parse_final_summary_stats.py --topo ${topo}_lndCap --payment-graph-type circ --credit-list 3200 \
        --demand 30 --scheme-list DCTCPQ --save ${topo}_lnd_credit3200_pathtypes_data --num-max 4 \
        --path-type-list oblivious kspYen widest
    
    python parse_final_summary_stats.py --topo ${topo}_lndCap --payment-graph-type circ --credit-list 3200 \
        --demand 30 --scheme-list DCTCPQ --save ${topo}_lnd_credit3200_pathnum_data --num-max 4 \
        --path-num-list 1 2 8

    for credit in "uniform" "gaussian" "lnd"
        # size succ stats
        suffix=""
        amt=800
        credit1=100
        credit2=200
        
        if [ $credit == "lnd" ]; then
            suffix="_lndCap"
            amt=3200
            credit1=6400
            credit2=12800
        elif [ $credit == "gaussian" ]; then
            suffix="_randomCap"
        fi

        # summary data
        python parse_final_summary_stats.py --topo ${topo}${suffix} --payment-graph-type circ \
            --credit-list $credit1 $credit2 400 800 1600 3200 --demand 30 \
            --scheme-list priceSchemeWindow waterfilling lndBaseline DCTCPQ landmarkRouting \
            shortestPath \
            --save ${topo}_${credit}_credit_data --num-max 4


        python parse_probability_size_stats.py --topo ${topo}${suffix} --payment-graph-type circ \
            --credit ${amt} --demand 30 --scheme-list waterfilling lndBaseline DCTCPQ landmarkRouting \
            shortestPath priceSchemeWindow --save ${topo}_${credit}_credit${amt}_prob_data --num-max 4 \
            --num-buckets 8
