#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
data<-read.csv(file)



# add a line plot
queue_plot <- ggplot(data, aes_string(x="time",y="value",color="src")) +
        geom_line(color="#d95f0e") +
        #coord_cartesian(xlim=c(-1,limit)) +
        labs(x="Time (s)", y="Queue Size (TU)") +

        theme_gray() + 
        #theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(15,"points"), 
        #      legend.position="bottom")

ggsave(plot_filename, width=4.3, height=3.2)
