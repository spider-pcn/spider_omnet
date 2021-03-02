#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
limit <- as.integer(args[3])
data<-read.csv(file)

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
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(10,"points"))

ggsave(paste(plot_filename,"SuccRatio.pdf", sep=""), width=5.7, height=5)

legend <- get_legend(succ_ratio_plot + theme(legend.position="top"))

# add a line plot
succ_volume_plot <- ggplot(data, aes_string(x="Demand",y="SuccVolume", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,limit),ylim=c(0,100)) +
        labs(x="Demand (TU/s)", y="Normalized Throughput (%)") +
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

        theme_minimal(base_size=22) 
ggsave(paste(plot_filename, "SuccVolume.pdf", sep=""), width=5.7, height=5)

# add a line plot for completion times
completion_time_plot <- ggplot(data, aes_string(x="Demand",y="CompletionTime", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,limit)) +
        labs(x="Demand (TU/s)", y="Completion Time (ms)") +
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

        theme_minimal(base_size=22) 
ggsave(paste(plot_filename,"CompletionTime.pdf", sep=""), width=5.7, height=5)

prow <- plot_grid(succ_ratio_plot + theme(legend.position="none"),
                  succ_volume_plot + theme(legend.position="none"), 
                  #completion_time_plot + theme(legend.position="none"),
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"Summary.pdf", sep=""), width=12.2, height=5)




