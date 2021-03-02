#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
data<-read.csv(file)
data$Topo <- factor(data$Topo, levels=c("sw", "sf", "lnd"), ordered=T)

label_list <- c("PS" = "Primal Dual Approach",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND",
                 "DCTCP" = "DCTCP (threshold)",
                 "arrival" = "Demand",
                 "DCTCP_qdelay" = "Spider")

break_list <- c("PS", "SP", "LR", "WF", "LND", "DCTCP", "DCTCP_qdelay", "arrival")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP" = "#a65628",
                "arrival" = "#000000",
                "DCTCP_qdelay" = "#0c2c84")

line_list <- c(
                "SP" = "solid",
                "PS" = "solid",
                "LR" = "solid",
                "WF" ="solid",
                "LND" = "solid",
                "DCTCP" = "solid",
                "arrival" = "dashed",
                "DCTCP_qdelay" = "solid")

topo_mapping <- c(
                   "sw" = "Small World",
                   "sf" = "Scale Free",
                   "lnd" = "Lightning Network")

labeller_func <- function(variable,value){
        return(topo_mapping[value])
}
# add a line plot
fairness_plot <- ggplot(data, aes_string(x="SuccVol",colour="Scheme",linetype="Scheme")) +
        stat_ecdf(size=1, geom='step') +
        scale_x_continuous(expand=c(0,0), trans="log2") +
        facet_wrap(. ~ Topo, labeller=labeller_func) +
        labs(x="Throughput of a flow (€/s)", y="CDF") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_linetype_manual(
                values=line_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +
        
        theme_minimal(base_size=22) + 
        
        theme(legend.text=element_text(size=rel(0.92)), legend.key.size=unit(18,"points"), legend.position="top", 
              legend.box.margin=margin(-10,-4,-10,-10),
              legend.margin=margin(c(5,0,5,0))) +
        theme(strip.text.x = element_text(size=20), strip.text.y = element_text(size=20))


ggsave(plot_filename, width=12, height=5)
