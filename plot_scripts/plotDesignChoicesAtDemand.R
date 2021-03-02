#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
file <- "spiderDesignDecisionsData"
plot_filename <- "LNDDesignChoicesDemand"
limit <- 420
data<-read.csv(file)

label_list <- c("Spider" = "Diffindo",
                 "NoWindow" = "Without Window",
                 "NoQueue" = "Without AQM",
                 "NoSplit" = "Without Packetization")

break_list <- c("Spider", "NoWindow", "NoQueue", "NoSplit")

color_list <- c(
                "Spider" = "#e51a1c",
                "NoWindow" = "#377eb8",
                "NoQueue" = "#4daf4a",
                "NoSplit" ="#984ea3")

shape_list <- c(8,19,18,4,15, 17)

# add a line plot
succ_ratio_plot <- ggplot(data, aes_string(x="Demand",y="SuccRatio", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,limit),ylim=c(0,100)) +
        labs(x="Demand (TU/s)", y="Success Ratio (%)") +
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

        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.7)), legend.key.size=unit(10,"points"), legend.position="top")

ggsave(paste(plot_filename,"SuccRatio.pdf", sep=""), width=5.7, height=5)

legend <- get_legend(succ_ratio_plot + theme(legend.position="top"))

# add a line plot
succ_volume_plot <- ggplot(data, aes_string(x="Demand",y="SuccessVolume", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,limit),ylim=c(0,100)) +
        labs(x="Demand (TU/s)", y="Normalized Throughput (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=2)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                guide=guide_legend(title=NULL, nrow=2)) +

        theme_minimal(base_size=22) +
        theme(legend.text=element_text(size=rel(0.7)), legend.key.size=unit(10,"points"), legend.position="top")
ggsave(paste(plot_filename, "SuccVolume.pdf", sep=""), width=5.7, height=5)


prow <- plot_grid(succ_ratio_plot + theme(legend.position="none"),
                  succ_volume_plot + theme(legend.position="none"), 
                  #completion_time_plot + theme(legend.position="none"),
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"Summary.pdf", sep=""), width=12.2, height=5)




