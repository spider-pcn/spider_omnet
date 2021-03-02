#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(scales)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
data<-read.csv(file)
limit <- as.integer(args[3])
data$Topo <- factor(data$Topo, levels=c("sw", "sf", "lnd"), ordered=T)

levels(data$Topo)

label_list <- c("PS" = "Primal Dual Approach",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND",
                 "DCTCP_qdelay" = "Spider",
                 "Circ" = "Circulation")

break_list <- c("PS", "SP", "LR", "WF", "LND", "DCTCP_qdelay", "Circ")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP_qdelay" = "#0c2c84",
                "Circ" = "#000000")

line_list <- c(
                "PS" = "solid",
                "LND" = "solid",
                "LR" = "solid",
                "DCTCP_qdelay" = "solid",
                "SP" = "solid",
                "WF" = "solid",
                "Circ" = "dashed")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP_qdelay" = 19,
                "Circ" = 17)

topo_mapping <- c(
                   "sw" = "Small World",
                   "sf" = "Scale Free",
                   "lnd" = "Lightning Network")


labeller_func <- function(variable,value){
        if (variable == "Topo") {
            return(topo_mapping[value])
        } else {
            return(credit_mapping[value])
        }
}

# add a line plot
succ_ratio_plot <- ggplot(
                        data[(data$Scheme != "Circ") & (data$Topo != "lnd"),], 
                        aes_string(x="DAGAmt",y="SuccRatio", colour="Scheme", 
                                         shape="Scheme", linetype="Scheme")) +
        geom_line(size=1) +
        facet_wrap(~ Topo, labeller=labeller_func) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(ylim=c(0,100), xlim=c(-1,limit)) +
        labs(x="DAG Amount (%)", y="Success Ratio (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        scale_linetype_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=line_list,
                    guide=guide_legend(title=NULL, nrow =1)) +
        
        theme_minimal(base_size=20) +

        # hack to convert facet_wrap into facet_grid 
        theme(axis.title.x = element_blank()) +
        theme(panel.spacing = unit(2, "lines")) +
        

        theme(plot.title = element_text(hjust = 0.5, vjust = -25, size=rel(0.8))) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(12,"points"), 
              legend.position="none", legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))


succ_volume_plot <- ggplot(
                        data[data$Topo != "lnd",],
                        aes_string(x="DAGAmt",y="SuccVolume", colour="Scheme", shape="Scheme", 
                                   linetype="Scheme")) +
        geom_line(size=1) +
        facet_wrap( ~ Topo, labeller=labeller_func) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccVolumeMin", ymax="SuccVolumeMax"), width=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(ylim=c(0,100), xlim=c(-1,limit)) +
        labs(x="DAG Amount (%)", y="Norm. Throughput (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        scale_linetype_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=line_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=19) +
        theme(panel.spacing = unit(2, "lines")) +
        theme(strip.background = element_blank(), legend.box.margin=margin(-10,-10,-10,-10),
                strip.text.x = element_blank(), legend.margin=margin(c(0,0,0,0))) + 
        theme(plot.title = element_text(hjust = 0.5, vjust = 0, size=rel(0.8))) + 
        theme(legend.text=element_text(size=rel(0.75)), legend.key.size=unit(12,"points"), legend.position="none")

legend <- get_legend(succ_volume_plot + theme(legend.position="top"))

p <- plot_grid(legend, succ_ratio_plot, succ_volume_plot, rel_heights=c(0.05, 0.9,0.9), nrow = 3)
ggsave(plot_filename, width=8.2, height=8)

# add a line plot
