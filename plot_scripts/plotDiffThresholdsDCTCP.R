#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
limit <- as.integer(args[3])
data<-read.csv(file)
data$Credit <- as.factor(data$Credit)
#head(data$Credit)


color_list <- c(
                "#c7e9b4",
                "#7fcdbb",
                "#41b6c4",
                "#1d91c0",
                "#225ea8",
                "#0c2c84")

shape_list <- c(8,19,18,4,15, 17, 34, 36)


# add a line plot
succ_ratio_plot <- ggplot(data, aes_string(x="Threshold",y="SuccRatio", colour="Credit", shape="Credit")) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0), trans="log2") +
        coord_cartesian(xlim=c(10,limit),ylim=c(0,100)) +
        labs(x="Threshold (ms)", y="Success Ratio (%)") +
        scale_colour_manual(
                values=color_list,
                guide=guide_legend(title=NULL, nrow=2)) +

        scale_shape_manual(
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =2)) +

        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(10,"points"), legend.position="top")

ggsave(paste(plot_filename,"SuccRatio.pdf", sep=""), width=5.7, height=5)

legend <- get_legend(succ_ratio_plot + theme(legend.position="top"))

# add a line plot
succ_volume_plot <- ggplot(data, aes(x=Threshold,y=SuccVolume, colour=Credit, shape=Credit)) +
        geom_line(size=1) +
        geom_point(size=3) +
        scale_x_continuous(expand=c(0,0),trans="log2") +
        coord_cartesian(xlim=c(10,limit),ylim=c(0,100)) +
        labs(x="Threshold (ms)", y="Normalized Throughput (%)") +
        scale_colour_manual(
                values=color_list,
                guide=guide_legend(title=NULL)) +

        scale_shape_manual(
                    values=shape_list,
                guide=guide_legend(title=NULL)) +

        theme_minimal(base_size=22) 
ggsave(paste(plot_filename, "SuccVolume.pdf", sep=""), width=5.7, height=5)


prow <- plot_grid(succ_ratio_plot + theme(legend.position="none"),
                  succ_volume_plot + theme(legend.position="none"), 
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"Summary.pdf", sep=""), width=12.2, height=5)




