#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
file <- "lndThroughputCapacityData"
plot_filename <- "lndThroughputCapacity.pdf"
limit <- 330
data<-read.csv(file)

data$Throughput <-  data$Throughput/102.0

label_list <- c("PS" = "Diffindo",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND")

break_list <- c("PS", "SP", "LR", "WF", "LND")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99")

shape_list <- c(8,19,18,4,15, 17)


# add a line plot
succ_volume_plot <- ggplot(data, aes_string(x="Capacity",y="Throughput", colour="paths", shape="paths")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(-1,limit)) +
        labs(x="Capacity Per Payment Channel (TU)", y="Throughput Per Sender (TU/s)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=2)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =2)) +

        theme_minimal(base_size=14) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(15,"points"), legend.position="top")

ggsave(plot_filename, width=4.3, height=3.2)
