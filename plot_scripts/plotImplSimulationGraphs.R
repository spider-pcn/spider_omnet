#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
limit <- 410
data<-read.csv(file)

label_list <- c("PS" = "Diffindo",
                 "SP" = "Shortest Path",
                 "LR" = "LND",
                 "WF" = "Waterfilling", 
                 "DCTCP_qdelay" = "Spider",
                 "LND" = "LND",
                 "Circ" = "Circulation")

break_list <- c("PS", "SP", "LR", "WF", "LND","Circ", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP_qdelay" = "#0c2c84",
                "Circ" = "#000000")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP_qdelay" = 19,
                "Circ" = 17)

scenario_mapping <- c(
                   "Impl" = "Implementation",
                   "Sim" = "Simulator")

labeller_func <- function(variable,value){
        return(scenario_mapping[value])
}

# add a line plot
succ_ratio_plot <- ggplot(data,
                          aes_string(x="Credit",y="SuccRatio", colour="Scheme", shape="Scheme")) +
        facet_wrap(~ Scenario, labeller=labeller_func) +
        geom_line(size=1) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(-1,limit),ylim=c(0,100)) +
        labs(x="Channel Size (€)", y="Success Ratio (%)") +
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
        
        theme_minimal(base_size=25) +
        theme(panel.spacing = unit(2, "lines")) +
        theme(axis.text.x=element_text(size=20)) +
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(12,"points"), 
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)),
              legend.position="top")

ggsave(plot_filename, width=12.2, height=4)
