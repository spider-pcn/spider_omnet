#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
args <- commandArgs(trailingOnly=TRUE)
file <- args[1] 
plot_filename <- args[2]
#plot_title <- args[3]
data<-read.csv(file)


label_list <- c("PS" = "Diffindo",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "Lightning Network")

break_list <- c("PS", "SP", "LR", "WF", "LND")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99")
y_axis <- "Normalized Throughput (%)"
pos <- "top"
height <- 3

# add a line plot
succ_ratio_plot <- ggplot(data, aes_string(x="creditDist", y="succVolume", fill="topo")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black", width=.8) +
        labs(x="Credit Distribution", y=y_axis) +
        scale_fill_manual(values=color_list,
                          labels=label_list,
                          breaks=break_list)+
        theme_minimal() +
        theme(legend.position=pos, legend.margin=margin(c(0,5,1,5)))+
        theme(legend.title=element_blank())


ggsave(plot_filename, width=6, height=height)
