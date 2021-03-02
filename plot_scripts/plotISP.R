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
succ_ratio_plot <- ggplot(data, aes_string(x="Credit",y="SuccRatio", colour="scheme", shape="scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,105000)) +
        labs(x="Capacity (XRP)", y="Success Ratio (%)") +
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

        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(10,"points"))

ggsave("ispSuccRatio.pdf", width=5.7, height=5)

legend <- get_legend(succ_ratio_plot + theme(legend.position="top"))

# add a line plot
succ_volume_plot <- ggplot(data, aes_string(x="Credit",y="SuccVolume", colour="scheme", shape="scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,105000)) +
        labs(x="Capacity (XRP)", y="Success Volume (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                guide=guide_legend(title=NULL)) +

        theme_minimal(base_size=22) +

ggsave("ispSuccVolume.pdf", width=5.7, height=5)

prow <- plot_grid(succ_ratio_plot + theme(legend.position="none"),
                  succ_volume_plot + theme(legend.position="none"), 
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave("ispSuccMetrics.pdf", width=12.2, height=5)




