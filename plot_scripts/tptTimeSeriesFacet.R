#!/usr/bin/env Rscript

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)

args <- commandArgs(trailingOnly=TRUE)
plot_filename <- args[1]
data <- read.csv("tpt_timeseries_data", sep=",")
data$topo <- factor(data$topo, levels=c("sw", "sf", "lnd"), ordered=T)

topo_mapping <- c(
                   "sw" = "Small World",
                   "sf" = "Scale Free",
                   "lnd" = "Lightning Network")

labeller_func <- function(variable,value){
        return(topo_mapping[value])
}

label_list <- c("circ" = "Pure Circulation",
                 "dagcirc" = "DAG + Circulation")

color_list <- c(
                "dagcirc" = "#e51a1c",
                "circ" = "#4daf4a")

tpt_plot <- ggplot(data, aes(x=time,y=tpt,color=type)) +
	geom_line(size=1) + 
    facet_wrap(~ topo, labeller=labeller_func) +
    ylim(0, 100) +
    xlim(0,3000) + 
    xlab("Time (s)") + ylab("Norm. Throughput(%)") +
    
    scale_colour_manual(
            values=color_list,
            labels=label_list,
            guide=guide_legend(title=NULL, nrow=1)) +
    
    theme_minimal(base_size=23) +
    theme(panel.spacing = unit(3, "lines")) +
    theme(axis.text.x=element_text(size=rel(1.1)), axis.text.y=element_text(size=rel(1.1))) + 
    theme(legend.text=element_text(size=rel(0.9)), legend.key.size=unit(20,"points"), legend.position="top",
          legend.box.margin=margin(-10,-10,-10,-10),
          legend.margin=margin(c(0,0,0,0)),
          strip.background = element_blank())

ggsave("dagThroughputTimeseries.pdf", width=12.2, height=4)
