#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
data<-read.csv(file)

label_list <- c("HEP" = "Spider (Waterfilling)",
                 "SHP" = "Shortest Path",
                 "MF" = "Max-flow",
                 "SW" = "SilentWhispers", 
                 "SM" = "SpeedyMurmurs", 
                 "BA" = "Spider (LP)")

break_list <- c("HEP", "MF", "SHP", "BA", "SW", "SM")

color_list <- c(
                "BA" = "#e51a1c",
                "HEP" = "#377eb8",
                "MF" = "#4daf4a",
                "SHP" ="#984ea3",
                "SM" = "#fb9a99",
                "SW" = "#ff7f00")

shape_list <- c(8,19,18,4,15, 17)

# add a line plot
heat_map_plot <- ggplot(data, aes_string(x="queueId",y="time", fill="value")) +
        geom_tile(color="red") +
        scale_y_reverse() +
        scale_fill_gradient(high = "#e34a33", low = "#fee8c8") +
        theme_minimal() + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(10,"points"))

ggsave("queueHeatMap.pdf", width=5.7, height=5)

legend <- get_legend(heat_map_plot + theme(legend.position="top"))





